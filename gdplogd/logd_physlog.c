/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**	----- END LICENSE BLOCK -----
*/

#include "logd.h"
#include "logd_circular_buffer.h"
#include "logd_physlog.h"

#include <gdp/gdp_buf.h>
#include <gdp/gdp_gclmd.h>

#include <ep/ep_hash.h>
#include <ep/ep_log.h>
#include <ep/ep_mem.h>
#include <ep/ep_net.h>
#include <ep/ep_thr.h>

#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/file.h>
#include <stdint.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.physlog",
								"GDP Log Daemon Physical Log");

#define GCL_PATH_MAX		200		// max length of pathname

static const char	*GCLDir;		// the gcl data directory

typedef struct index_entry
{
	// reading and writing to the log requires holding this lock
	EP_THR_RWLOCK		lock;
	FILE				*fp;
	int64_t				max_recno;
	int64_t				max_data_offset;
	int64_t				max_index_offset;
	CIRCULAR_BUFFER		*index_cache;
} gcl_log_index_t;


/*
**  FSIZEOF --- return the size of a file
*/

static off_t
fsizeof(FILE *fp)
{
	struct stat st;

	if (fstat(fileno(fp), &st) < 0)
	{
		char errnobuf[200];

		strerror_r(errno, errnobuf, sizeof errnobuf);
		ep_dbg_cprintf(Dbg, 1, "fsizeof: fstat failure: %s\n", errnobuf);
		return -1;
	}

	return st.st_size;
}


/*
**  POSIX_ERROR --- flag error caused by a Posix (Unix) syscall
*/

static EP_STAT EP_TYPE_PRINTFLIKE(2, 3)
posix_error(int _errno, const char *fmt, ...)
{
	va_list ap;
	EP_STAT estat = ep_stat_from_errno(_errno);

	va_start(ap, fmt);
	ep_logv(estat, fmt, ap);
	va_end(ap);

	return estat;
}


EP_STAT
gcl_physlog_init()
{
	EP_STAT estat = EP_STAT_OK;

	// find physical location of GCL directory
	GCLDir = ep_adm_getstrparam("swarm.gdplogd.gcl.dir", GCL_DIR);

	return estat;
}



/*
**	GET_GCL_PATH --- get the pathname to an on-disk version of the gcl
**
**		XXX	Once we start having a lot of GCLs, we should probably
**			create subdirectories based on (at least) the first octet
**			of the name, possibly more.
*/

static EP_STAT
get_gcl_path(gdp_gcl_t *gcl, const char *ext, char *pbuf, int pbufsiz)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_pname_t pname;
	int i;

	EP_ASSERT_POINTER_VALID(gcl);

	errno = 0;
	gdp_printable_name(gcl->name, pname);

	// find the subdirectory based on the first part of the name
	{
		struct stat st;

		i = snprintf(pbuf, pbufsiz, "%s/_%02x",
					GCLDir, gcl->name[0]);
		if (i >= pbufsiz)
		{
			estat = EP_STAT_BUF_OVERFLOW;
			goto fail0;
		}
		if (stat(pbuf, &st) < 0)
		{
			// doesn't exist; we need to create it
			ep_dbg_cprintf(Dbg, 10, "get_gcl_path: creating %s\n", pbuf);
			i = mkdir(pbuf, 0775);
			if (i < 0)
				goto fail0;
		}
		else if ((st.st_mode & S_IFMT) != S_IFDIR)
		{
			errno = ENOTDIR;
			goto fail0;
		}
	}

	i = snprintf(pbuf, pbufsiz, "%s/_%02x/%s%s",
				GCLDir, gcl->name[0], pname, ext);
	if (i < pbufsiz)
		return EP_STAT_OK;

fail0:
	{
		char ebuf[100];

		if (!EP_STAT_ISOK(estat))
		{
			if (errno == 0)
				estat = EP_STAT_ERROR;
			else
				estat = ep_stat_from_errno(errno);
		}

		ep_dbg_cprintf(Dbg, 1, "get_gcl_path(%s):\n\t%s\n",
				pbuf, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  The index cache gives you offsets into a GCL by record number.
**
**		XXX	The circular buffer implementation seems a bit dubious,
**			since it's perfectly reasonable for someone to be reading
**			the beginning of a large cache while data is being written
**			at the end, which will cause thrashing.
*/

EP_STAT
gcl_index_create_cache(gdp_gcl_t *gcl, gcl_log_index_t **out)
{
	gcl_log_index_t *index = ep_mem_malloc(sizeof *index);

	if (index == NULL)
		goto fail0;
	if (ep_thr_rwlock_init(&index->lock) != 0)
		goto fail1;
	index->fp = NULL;
	index->max_recno = 0;
	index->max_data_offset = gcl->x->data_offset;
	index->max_index_offset = SIZEOF_INDEX_HEADER;
	int cache_size = ep_adm_getintparam("swarm.gdplogd.index.cachesize", 65536);
								// 1 MiB index cache
	index->index_cache = circular_buffer_new(cache_size);
	*out = index;
	return EP_STAT_OK;

fail1:
	free(index);
fail0:
	return EP_STAT_ERROR;
}

EP_STAT
gcl_index_cache_get(gcl_log_index_t *entry, int64_t recno, int64_t *out)
{
	EP_STAT estat = EP_STAT_OK;

	ep_thr_rwlock_rdlock(&entry->lock);
	LONG_LONG_PAIR *found = circular_buffer_search(entry->index_cache, recno);
	if (found != NULL)
	{
		*out = found->value;
	}
	else
	{
		*out = INT64_MAX;
	}
	ep_thr_rwlock_unlock(&entry->lock);

	return estat;
}

EP_STAT
gcl_index_cache_put(gcl_log_index_t *entry, int64_t recno, int64_t offset)
{
	EP_STAT estat = EP_STAT_OK;
	LONG_LONG_PAIR new_pair;

	new_pair.key = recno;
	new_pair.value = offset;

	circular_buffer_append(entry->index_cache, new_pair);

	return estat;
}


/*
**  GCL_PHYSCREATE --- create a brand new GCL on disk
*/

EP_STAT
gcl_physcreate(gdp_gcl_t *gcl, gdp_gclmd_t *gmd)
{
	EP_STAT estat = EP_STAT_OK;
	char data_pbuf[GCL_PATH_MAX];
	FILE *data_fp;
	FILE *index_fp;

	// allocate a name
	if (!gdp_name_is_valid(gcl->name))
	{
		_gdp_gcl_newname(gcl);
	}

	// create a file node representing the gcl
	{
		int data_fd;

		estat = get_gcl_path(gcl, GCL_DATA_SUFFIX,
						data_pbuf, sizeof data_pbuf);
		EP_STAT_CHECK(estat, goto fail1);

		data_fd = open(data_pbuf, O_RDWR | O_CREAT | O_APPEND | O_EXCL, 0644);
		if (data_fd < 0 || (flock(data_fd, LOCK_EX) < 0))
		{
			estat = posix_error(errno, "gcl_create: cannot create %s",
					data_pbuf);
			goto fail1;
		}
		data_fp = fdopen(data_fd, "a+");
		if (data_fp == NULL)
		{
			estat = posix_error(errno, "gcl_create: cannot fdopen %s",
					data_pbuf);
			(void) close(data_fd);
			goto fail2;
		}
	}

	// create an offset index for that gcl
	{
		int index_fd;
		char index_pbuf[GCL_PATH_MAX];

		estat = get_gcl_path(gcl, GCL_INDEX_SUFFIX,
						index_pbuf, sizeof index_pbuf);
		EP_STAT_CHECK(estat, goto fail2);

		index_fd = open(index_pbuf, O_RDWR | O_CREAT | O_APPEND | O_EXCL, 0644);
		if (index_fd < 0)
		{
			estat = posix_error(errno, "gcl_create: cannot create %s",
				index_pbuf);
			goto fail2;
		}
		index_fp = fdopen(index_fd, "a+");
		if (index_fp == NULL)
		{
			estat = posix_error(errno, "gcl_create: cannot fdopen %s",
				index_pbuf);
			(void) close(index_fd);
			goto fail2;
		}
	}

	// write the header
	{
		gcl_log_header log_header;
		size_t metadata_size = 0; // XXX: compute size of metadata
		int i;

		metadata_size = 0;
		if (gmd == NULL)
		{
			log_header.num_metadata_entries = 0;
		}
		else
		{
			// allow space for id and length fields
			metadata_size = gmd->nused * 2 * sizeof (uint32_t);
			gcl->x->nmetadata = gmd->nused;
			log_header.num_metadata_entries = ep_net_hton16(gmd->nused);

			// compute the space needed for the data fields
			for (i = 0; i < gmd->nused; i++)
				metadata_size += gmd->mds[i].md_len;
		}

		gcl->x->ver = GCL_LOG_VERSION;
		gcl->x->data_offset = sizeof (gcl_log_header) + metadata_size;

		log_header.magic = ep_net_hton32(GCL_LOG_MAGIC);
		log_header.version = ep_net_hton32(GCL_LOG_VERSION);
		log_header.header_size = ep_net_ntoh32(gcl->x->data_offset);
		log_header.reserved1 = 0;
		log_header.log_type = ep_net_hton16(0);		//XXX unused for now
		log_header.extent = ep_net_hton16(0);		//XXX unused for now
		log_header.reserved2 = 0;
		memcpy(log_header.gname, gcl->name, sizeof log_header.gname);
		log_header.recno_offset = 0;

		fwrite(&log_header, sizeof(log_header), 1, data_fp);
	}

	// write metadata
	if (gmd != NULL)
	{
		int i;

		// first the id and length fields
		for (i = 0; i < gmd->nused; i++)
		{
			uint32_t t32;

			t32 = ep_net_hton32(gmd->mds[i].md_id);
			fwrite(&t32, sizeof t32, 1, data_fp);
			t32 = ep_net_hton32(gmd->mds[i].md_len);
			fwrite(&t32, sizeof t32, 1, data_fp);
		}

		// ... then the actual data
		for (i = 0; i < gmd->nused; i++)
		{
			fwrite(gmd->mds[i].md_data, gmd->mds[i].md_len, 1, data_fp);
		}
	}

	gcl_log_index_t *new_index = NULL;
	estat = gcl_index_create_cache(gcl, &new_index);
	EP_STAT_CHECK(estat, goto fail3);

	// success!
	if (fflush(data_fp) < 0 || ferror(data_fp))
	{
		estat = posix_error(errno, "gcl_create: cannot flush data file %s",
						data_pbuf);
		goto fail3;
	}

	flock(fileno(data_fp), LOCK_UN);
	new_index->fp = index_fp;
	gcl->x->fp = data_fp;
	gcl->x->log_index = new_index;
	ep_dbg_cprintf(Dbg, 10, "Created GCL Handle %s\n", gcl->pname);
	return estat;

fail3:
	fclose(index_fp);
fail2:
	flock(fileno(data_fp), LOCK_UN);
	fclose(data_fp);
fail1:
	// turn "file exists" into a meaningful response code
	if (EP_STAT_IS_SAME(estat, ep_stat_from_errno(EEXIST)))
			estat = GDP_STAT_NAK_CONFLICT;

	if (ep_dbg_test(Dbg, 1))
	{
		char ebuf[100];

		ep_dbg_printf("Could not create GCL Handle: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	GCL_PHYSOPEN --- do physical open of a GCL
**
**		XXX: Should really specify whether we want to start reading:
**		(a) At the beginning of the log (easy).	 This includes random
**			access.
**		(b) Anything new that comes into the log after it is opened.
**			To do this we need to read the existing log to find the end.
*/

EP_STAT
gcl_physopen(gdp_gcl_t *gcl)
{
	EP_STAT estat = EP_STAT_OK;
	int fd;
	FILE *data_fp;
	FILE *index_fp;
	char data_pbuf[GCL_PATH_MAX];
	char index_pbuf[GCL_PATH_MAX];

	estat = get_gcl_path(gcl, GCL_DATA_SUFFIX, data_pbuf, sizeof data_pbuf);
	EP_STAT_CHECK(estat, goto fail0);

	estat = get_gcl_path(gcl, GCL_INDEX_SUFFIX, index_pbuf, sizeof index_pbuf);
	EP_STAT_CHECK(estat, goto fail0);

	fd = open(data_pbuf, O_RDWR | O_APPEND);
	if (fd < 0 || flock(fd, LOCK_SH) < 0 ||
			(data_fp = fdopen(fd, "a+")) == NULL)
	{
		estat = ep_stat_from_errno(errno);
		ep_dbg_cprintf(Dbg, 1, "gcl_physopen(%s): %s\n",
				data_pbuf, strerror(errno));
		if (fd >= 0)
			close(fd);
		goto fail1;
	}

	gcl_log_header log_header;
	rewind(data_fp);
	if (fread(&log_header, sizeof(log_header), 1, data_fp) < 1)
	{
		estat = posix_error(errno, "gcl_physopen(%s): header read failure",
						data_pbuf);
		goto fail3;
	}

	// on-disk format in network byte order
	log_header.magic = ep_net_ntoh32(log_header.magic);
	log_header.version = ep_net_ntoh32(log_header.version);
	log_header.header_size = ep_net_ntoh32(log_header.header_size);
	log_header.num_metadata_entries = ep_net_ntoh16(log_header.num_metadata_entries);
	log_header.log_type = ep_net_ntoh16(log_header.log_type);
	log_header.recno_offset = ep_net_ntoh64(log_header.recno_offset);

	if (log_header.magic != GCL_LOG_MAGIC)
	{
		estat = GDP_STAT_CORRUPT_GCL;
		ep_log(estat, "gcl_physopen: bad magic: found: %" PRIx32
				", expected: %" PRIx32 "\n",
				log_header.magic, GCL_LOG_MAGIC);
		goto fail3;
	}

	if (log_header.version < GCL_LOG_MINVERS ||
			log_header.version > GCL_LOG_MAXVERS)
	{
		estat = GDP_STAT_GCL_VERSION_MISMATCH;
		ep_log(estat, "gcl_physopen: bad version: found: %" PRIx32
				", expected: %" PRIx32 "-%" PRIx32 "\n",
				log_header.version, GCL_LOG_MINVERS, GCL_LOG_MAXVERS);
		goto fail3;
	}

	gcl->x->nmetadata = log_header.num_metadata_entries;
	gcl->x->log_type = log_header.log_type;
	gcl->x->recno_offset = log_header.recno_offset;

	// XXX: read metadata entries
	if (log_header.num_metadata_entries > 0)
	{
		int mdtotal = 0;
		void *md_data;
		int i;

		gcl->gclmd = gdp_gclmd_new(log_header.num_metadata_entries);
		for (i = 0; i < log_header.num_metadata_entries; i++)
		{
			uint32_t md_id;
			uint32_t md_len;

			if (fread(&md_id, sizeof md_id, 1, data_fp) != 1 ||
				fread(&md_len, sizeof md_len, 1, data_fp) != 1)
			{
				estat = GDP_STAT_GCL_READ_ERROR;
				goto fail3;
			}

			md_id = ntohl(md_id);
			md_len = ntohl(md_len);

			gdp_gclmd_add(gcl->gclmd, md_id, md_len, NULL);
			mdtotal += md_len;
		}
		md_data = ep_mem_malloc(mdtotal);
		if (fread(md_data, mdtotal, 1, data_fp) != 1)
		{
			estat = GDP_STAT_GCL_READ_ERROR;
			goto fail3;
		}
		_gdp_gclmd_adddata(gcl->gclmd, md_data);
	}

	// open the index file
	fd = open(index_pbuf, O_RDWR | O_APPEND);
	if (fd < 0 || flock(fd, LOCK_SH) < 0 ||
			(index_fp = fdopen(fd, "a+")) == NULL)
	{
		estat = posix_error(errno, "gcl_physopen(%s): index open failure",
						index_pbuf);
		if (fd >= 0)
			close(fd);
		goto fail4;
	}

	// XXX should check for index header here

	gcl->x->ver = log_header.version;
	gcl->x->data_offset = log_header.header_size;

	gcl_log_index_t *index;
	estat = gcl_index_create_cache(gcl, &index);
	EP_STAT_CHECK(estat, goto fail5);

	gcl->x->fp = data_fp;
	gcl->x->log_index = index;

	index->fp = index_fp;
	index->max_data_offset = fsizeof(data_fp);
	index->max_index_offset = fsizeof(index_fp);
	index->max_recno = (index->max_index_offset - SIZEOF_INDEX_HEADER)
								/ SIZEOF_INDEX_RECORD;

	gcl->nrecs = index->max_recno;

	return estat;

fail5:
fail4:
fail3:
	fclose(data_fp);
fail1:
fail0:
	// map errnos to appropriate NAK codes
	if (EP_STAT_IS_SAME(estat, ep_stat_from_errno(ENOENT)))
		estat = GDP_STAT_NAK_NOTFOUND;

	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];

		ep_dbg_printf("Couldn't open gcl %s:\n\t%s\n",
				gcl->pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}

/*
**	GCL_PHYSCLOSE --- physically close an open gcl
*/

EP_STAT
gcl_physclose(gdp_gcl_t *gcl)
{
	EP_STAT estat = EP_STAT_OK;

	EP_ASSERT_POINTER_VALID(gcl);

	// close the data file
	if (gcl->x->fp == NULL)
	{
		estat = GDP_STAT_INTERNAL_ERROR;
		ep_log(estat, "gcl_physclose: null fp");
	}
	else
	{
		if (fclose(gcl->x->fp) < 0)
			estat = posix_error(errno, "gcl_physclose: fclose (data)");
		gcl->x->fp = NULL;
	}

	// and the index file
	if (gcl->x->log_index->fp != NULL)
	{
		if (fclose(gcl->x->log_index->fp) < 0)
			estat = posix_error(errno, "gcl_physclose: fclose (index)");
		gcl->x->log_index->fp = NULL;
	}

	// now free up the associated memory
	ep_mem_free(gcl->x->log_index);
	ep_mem_free(gcl->x);
	gcl->x = NULL;

	return estat;
}


/*
**	GCL_PHYSREAD --- read a message from a gcl
**
**		Reads in a message indicated by datum->recno into datum.
**
**		In theory we should be positioned at the head of the next message.
**		But that might not be the correct message.	If we have specified a
**		message number there are two cases:
**			(a) We've already seen the message -> use the cached offset.
**				Of course we check to see if we got the record we expected.
**			(b) We haven't (yet) seen the message in this GCL Handle -> starting
**				from the last message we have seen, read up to this message,
**				assuming it exists.
*/

EP_STAT
gcl_physread(gdp_gcl_t *gcl,
		gdp_datum_t *datum)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_log_index_t *entry = gcl->x->log_index;
	LONG_LONG_PAIR *long_pair;
	int64_t offset = INT64_MAX;

	EP_ASSERT_POINTER_VALID(gcl);

	ep_dbg_cprintf(Dbg, 14, "gcl_physread(%" PRIgdp_recno "): ", datum->recno);

	ep_thr_rwlock_rdlock(&entry->lock);

	// first check if recno is in the index
	long_pair = circular_buffer_search(entry->index_cache, datum->recno);
	if (long_pair == NULL)
	{
		// recno is not in the index
		// now binary search through the disk index
		flockfile(entry->fp);

		off_t file_size = fsizeof(entry->fp);
		if (file_size < 0)
		{
			estat = ep_stat_from_errno(errno);
			ep_log(estat, "gcl_physread: fsizeof failed");
			goto fail1;
		}
		if (file_size < SIZEOF_INDEX_HEADER)
		{
			estat = GDP_STAT_CORRUPT_INDEX;
			goto fail1;
		}
		int64_t record_count = (file_size - SIZEOF_INDEX_HEADER) /
								SIZEOF_INDEX_RECORD;
		gcl_index_record index_record;
		size_t start = 0;
		size_t end = record_count;
		size_t mid;

		ep_dbg_cprintf(Dbg, 14,
				"searching index, rec_count=%" PRId64 ", file_size=%ju\n",
				record_count, (uintmax_t) file_size);

		/*
		**  XXX Why can't this just be computed from the record number?
		**		The on-disk format has to start at recno 1 (in fact, the
		**		algorithm assumes that).  For that matter, the file doesn't
		**		need the recno stored at all --- just take the offset
		**		into the index file based on recno to get the offset into
		**		the data file.
		**
		**		There is a possibility that the recnos may not start from
		**		1.  But this is easily solved by including a header on the
		**		index file (which should be there anyway) and storing the
		**		initial recno in the header.  Either way it should be easy
		**		to compute the index file offset without searching.
		*/

		while (start < end)
		{
			mid = start + (end - start) / 2;
			ep_dbg_cprintf(Dbg, 47, "\t%zu ... %zu ... %zu\n", start, mid, end);
			if (fseek(entry->fp, mid * sizeof(gcl_index_record), SEEK_SET) < 0)
			{
				estat = posix_error(errno, "gcl_physread: fseek failed");
				funlockfile(entry->fp);
				goto fail0;
			}
			if (fread(&index_record, sizeof(gcl_index_record), 1, entry->fp) < 1)
			{
				estat = posix_error(errno, "gcl_physread: fread failed");
				funlockfile(entry->fp);
				goto fail0;
			}

			index_record.recno = ep_net_ntoh64(index_record.recno);
			index_record.offset = ep_net_ntoh64(index_record.offset);

			if (datum->recno < index_record.recno)
			{
				end = mid;
			}
			else if (datum->recno > index_record.recno)
			{
				start = mid + 1;
			}
			else
			{
				offset = index_record.offset;
				break;
			}
		}
		funlockfile(entry->fp);
	}
	else
	{
		offset = long_pair->value;
		ep_dbg_cprintf(Dbg, 14, "found in memory at %" PRId64 "\n", offset);
	}

	if (offset == INT64_MAX) // didn't find message
	{
		estat = EP_STAT_END_OF_FILE;
		goto fail0;
	}

	char read_buffer[GCL_READ_BUFFER_SIZE];
	gcl_log_record log_record;

	// read header
	flockfile(gcl->x->fp);
	if (fseek(gcl->x->fp, offset, SEEK_SET) < 0 ||
			fread(&log_record, sizeof(log_record), 1, gcl->x->fp) < 1)
	{
		estat = posix_error(errno, "gcl_physread: header fread failed");
		goto fail1;
	}
	offset += sizeof log_record;

	log_record.recno = ep_net_ntoh64(log_record.recno);
	ep_net_ntoh_timespec(&log_record.timestamp);
	log_record.sigmeta = ep_net_ntoh16(log_record.sigmeta);
	log_record.data_length = ep_net_ntoh64(log_record.data_length);

	ep_dbg_cprintf(Dbg, 29, "gcl_physread: recno %" PRIgdp_recno
				", sigmeta 0x%x, dlen %" PRId64 ", offset %" PRId64 "\n",
				log_record.recno, log_record.sigmeta, log_record.data_length,
				offset);

	datum->recno = log_record.recno;
	memcpy(&datum->ts, &log_record.timestamp, sizeof datum->ts);
	datum->sigmdalg = (log_record.sigmeta >> 12) & 0x000f;
	datum->siglen = log_record.sigmeta & 0x0fff;


	// read data in chunks and add it to the evbuffer
	int64_t data_length = log_record.data_length;
	char *phase = "data";
	while (data_length >= sizeof(read_buffer))
	{
		if (fread(read_buffer, sizeof(read_buffer), 1, gcl->x->fp) < 1)
			goto fail2;
		gdp_buf_write(datum->dbuf, read_buffer, sizeof read_buffer);
		data_length -= sizeof read_buffer;
	}
	if (data_length > 0)
	{
		if (fread(read_buffer, data_length, 1, gcl->x->fp) < 1)
			goto fail2;
		gdp_buf_write(datum->dbuf, read_buffer, data_length);
	}

	// read signature
	if (datum->siglen > 0)
	{
		phase = "signature";
		if (datum->siglen > sizeof read_buffer)
		{
			estat = GDP_STAT_INTERNAL_ERROR;
			ep_log(estat,
					"gcl_physread: datum->siglen = %d, sizeof read_buffer = %zd",
					datum->siglen, sizeof read_buffer);
			EP_ASSERT_INSIST(datum->siglen <= sizeof read_buffer);
		}
		if (datum->sig == NULL)
			datum->sig = gdp_buf_new();
		else
			gdp_buf_reset(datum->sig);
		if (fread(read_buffer, datum->siglen, 1, gcl->x->fp) < 1)
			goto fail2;
		gdp_buf_write(datum->sig, read_buffer, datum->siglen);
	}

	// done

	if (false)
	{
fail2:
		estat = posix_error(errno, "gcl_physread: %s fread failed", phase);
	}
fail1:
	funlockfile(gcl->x->fp);
fail0:
	ep_thr_rwlock_unlock(&entry->lock);

	return estat;
}

/*
**	GCL_PHYSAPPEND --- append a message to a writable gcl
*/

EP_STAT
gcl_physappend(gdp_gcl_t *gcl,
			gdp_datum_t *datum)
{
	gcl_log_record log_record;
	gcl_index_record index_record;
	size_t dlen = evbuffer_get_length(datum->dbuf);
	int64_t record_size = sizeof(gcl_log_record);
	gcl_log_index_t *entry = gcl->x->log_index;
	EP_STAT estat = EP_STAT_OK;

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("gcl_physappend ");
		_gdp_datum_dump(datum, ep_dbg_getfile());
	}

	ep_thr_rwlock_wrlock(&entry->lock);

	memset(&log_record, 0, sizeof log_record);
	log_record.recno = ep_net_hton64(entry->max_recno + 1);
	log_record.timestamp = datum->ts;
	ep_net_hton_timespec(&log_record.timestamp);
	log_record.data_length = ep_net_hton64(dlen);
	log_record.sigmeta = (datum->siglen & 0x0fff) |
				((datum->sigmdalg & 0x000f) << 12);
	log_record.sigmeta = ep_net_hton16(log_record.sigmeta);

	// write log record header
	fwrite(&log_record, sizeof(log_record), 1, gcl->x->fp);

	// write log record data
	if (dlen > 0)
	{
		unsigned char *p = evbuffer_pullup(datum->dbuf, dlen);
		if (p != NULL)
			fwrite(p, dlen, 1, gcl->x->fp);
		record_size += dlen;
	}

	// write signature
	if (datum->sig != NULL)
	{
		size_t slen = evbuffer_get_length(datum->sig);
		unsigned char *p = evbuffer_pullup(datum->sig, slen);
		EP_ASSERT_INSIST(datum->siglen == slen);
		if (slen > 0 && p != NULL)
			fwrite(p, slen, 1, gcl->x->fp);
		record_size += slen;
	}
	else if (datum->siglen > 0)
	{
		// "can't happen"
		ep_app_abort("gcl_physappend: siglen = %d but no signature",
				datum->siglen);
	}

	index_record.recno = log_record.recno;
	index_record.offset = ep_net_hton64(entry->max_data_offset);
	index_record.extent = 0;		//XXX someday

	// write index record
	fwrite(&index_record, sizeof(index_record), 1, entry->fp);

	// commit
	if (fflush(gcl->x->fp) < 0 || ferror(gcl->x->fp))
		estat = posix_error(errno, "gcl_physappend: cannot flush data");
	else if (fflush(entry->fp) < 0 || ferror(gcl->x->fp))
		estat = posix_error(errno, "gcl_physappend: cannot flush index");
	else
	{
		gcl_index_cache_put(entry, index_record.recno, index_record.offset);
		++entry->max_recno;
		entry->max_index_offset += sizeof(index_record);
		entry->max_data_offset += record_size;
	}

	ep_thr_rwlock_unlock(&entry->lock);

	return estat;
}


/*
**  GCL_PHYSGETMETADATA --- read metadata from disk
**
**		This is depressingly similar to _gdp_gclmd_deserialize.
*/

#define STDIOCHECK(tag, targ, f)	\
			do	\
			{	\
				int _t = f;	\
				if (_t != targ)	\
				{	\
					estat = posix_error(errno,	\
							"%s: stdio failure; expected %d got %d [%s]",	\
							tag, targ, _t, #f);	\
					goto fail_stdio;	\
				}	\
			} while (0);

EP_STAT
gcl_physgetmetadata(gdp_gcl_t *gcl,
		gdp_gclmd_t **gmdp)
{
	gdp_gclmd_t *gmd;
	int i;
	size_t tlen;
	gcl_log_index_t *entry = gcl->x->log_index;
	EP_STAT estat = EP_STAT_OK;

	ep_dbg_cprintf(Dbg, 29, "gcl_physgetmetadata: nmetadata %d\n",
			gcl->x->nmetadata);

	// allocate and populate the header
	gmd = ep_mem_zalloc(sizeof *gmd);
	gmd->flags = GCLMDF_READONLY;
	gmd->nalloc = gmd->nused = gcl->x->nmetadata;
	gmd->mds = ep_mem_malloc(gmd->nalloc * sizeof *gmd->mds);

	// lock the GCL so that no one else seeks around on us
	ep_thr_rwlock_rdlock(&entry->lock);

	// seek to the metadata area
	STDIOCHECK("gcl_physgetmetadata: fseek#0", 0,
			fseek(gcl->x->fp, sizeof (struct gcl_log_header), SEEK_SET));

	// read in the individual metadata headers
	tlen = 0;
	for (i = 0; i < gmd->nused; i++)
	{
		uint32_t t32;

		STDIOCHECK("gcl_physgetmetadata: fread#0", 1,
				fread(&t32, sizeof t32, 1, gcl->x->fp));
		gmd->mds[i].md_id = t32;
		STDIOCHECK("gcl_physgetmetadata: fread#1", 1,
				fread(&t32, sizeof t32, 1, gcl->x->fp));
		gmd->mds[i].md_len = t32;
		tlen += t32;
		ep_dbg_cprintf(Dbg, 34, "\tid = %08x, len = %zd\n",
				gmd->mds[i].md_id, gmd->mds[i].md_len);
	}

	ep_dbg_cprintf(Dbg, 24, "gcl_physgetmetadata: nused = %d, tlen = %zd\n",
			gmd->nused, tlen);

	// now the data
	gmd->databuf = ep_mem_malloc(tlen);
	STDIOCHECK("gcl_physgetmetadata: fread#2", 1,
			fread(gmd->databuf, tlen, 1, gcl->x->fp));

	// now map the pointers to the data
	void *dbuf = gmd->databuf;
	for (i = 0; i < gmd->nused; i++)
	{
		gmd->mds[i].md_data = dbuf;
		dbuf += gmd->mds[i].md_len;
	}

	*gmdp = gmd;

	if (false)
	{
fail_stdio:
		// well that's not very good...
		if (gmd->databuf != NULL)
			ep_mem_free(gmd->databuf);
		ep_mem_free(gmd->mds);
		ep_mem_free(gmd);
		estat = GDP_STAT_CORRUPT_GCL;
	}

	ep_thr_rwlock_unlock(&entry->lock);
	return estat;
}


/*
**  GCL_PHYSFOREACH --- call function for each GCL in directory
*/

void
gcl_physforeach(void (*func)(gdp_name_t, void *), void *ctx)
{
	int subdir;

	for (subdir = 0; subdir < 0x100; subdir++)
	{
		DIR *dir;
		char dbuf[400];

		snprintf(dbuf, sizeof dbuf, "%s/_%02x", GCLDir, subdir);
		dir = opendir(dbuf);
		if (dir == NULL)
			continue;

		for (;;)
		{
			struct dirent dentbuf;
			struct dirent *dent;

			// read the next directory entry
			int i = readdir_r(dir, &dentbuf, &dent);
			if (i != 0)
			{
				ep_log(ep_stat_from_errno(i),
						"gcl_physforeach: readdir_r failed");
				break;
			}
			if (dent == NULL)
				break;

			// we're only interested in .data files
			char *p = strrchr(dent->d_name, '.');
			if (p == NULL || strcmp(p, GCL_DATA_SUFFIX) != 0)
				continue;

			// strip off the ".data"
			*p = '\0';

			// convert the base64-encoded name to internal form
			gdp_name_t gname;
			EP_STAT estat = gdp_internal_name(dent->d_name, gname);
			EP_STAT_CHECK(estat, continue);

			// now call the function
			(*func)((uint8_t *) gname, ctx);
		}
		closedir(dir);
	}
}
