/* vim: set ai sw=4 sts=4 ts=4 : */

#include "gdp_circular_buffer.h"
#include "gdpd.h"
#include "gdpd_physlog.h"

#include <gdp/gdp_buf.h>

#include <ep/ep_hash.h>
#include <ep/ep_mem.h>

//#include <linux/limits.h>		XXX NOT PORTABLE!!!
#include <sys/file.h>
#include <sys/stat.h>
#include <ep/ep_thr.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/file.h>
#include <stdint.h>

/************************  PRIVATE	************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gdpd.physlog",
								"GDP Daemon Physical Log");

#define GCL_PATH_MAX		200		// max length of pathname
#define GCL_DIR				"/var/tmp/gcl"

#define GCL_NEXT_MSG		(-1)	// sentinel for next available message

static const char	*GCLDir;		// the gcl data directory
static EP_HASH		*name_index_table = NULL;

// reading and writing to the EP_HASH mapping gcl_name -> index requires
//		holding table_mutex
// EP_HASH is not thread-safe
static EP_THR_RWLOCK table_lock EP_THR_RWLOCK_INITIALIZER;

typedef struct index_entry
{
	// reading and writing to the log requires holding this lock
	pthread_rwlock_t	lock;
	FILE				*fp;
	int64_t				max_recno;
	int64_t				max_data_offset;
	int64_t				max_index_offset;
	CIRCULAR_BUFFER		*index_cache;
} gcl_log_index;

EP_STAT
gcl_physlog_init()
{
	EP_STAT estat = EP_STAT_OK;

	GCLDir = ep_adm_getstrparam("swarm.gdp.gcl.dir", GCL_DIR);
	name_index_table = ep_hash_new("name_index_table", NULL,
			sizeof(gcl_name_t));

	return estat;
}

static EP_STAT
gcl_log_index_new(gcl_handle_t *gclh, gcl_log_index **out)
{
	gcl_log_index *new_index = malloc(sizeof(gcl_log_index));
	if (new_index == NULL)
	{
		return EP_STAT_ERROR;
	}
	if (pthread_rwlock_init(&new_index->lock, NULL) != 0)
	{
		goto fail0;
	}
	new_index->fp = NULL;
	new_index->max_recno = 0;
	new_index->max_data_offset = gclh->data_offset;
	new_index->max_index_offset = 0;
	int cache_size = ep_adm_getintparam("swarm.gdp.index.cachesize", 65536);
								// 1 MiB index cache
	new_index->index_cache = circular_buffer_new(cache_size);
	*out = new_index;
	return EP_STAT_OK;

fail0:
	free(new_index);
	return EP_STAT_ERROR;
}


/*
**	GET_GCL_PATH --- get the pathname to an on-disk version of the gcl
*/

static EP_STAT
get_gcl_path(gcl_handle_t *gclh, char *pbuf, int pbufsiz)
{
	gcl_pname_t pname;
	int i;

	EP_ASSERT_POINTER_VALID(gclh);

	gdp_gcl_printable_name(gclh->gcl_name, pname);
	i = snprintf(pbuf, pbufsiz, "%s/%s", GCLDir, pname);
	if (i < pbufsiz)
		return EP_STAT_OK;
	else
		return EP_STAT_ERROR;
}


/*
**  Implement the index cache
**
**		XXX	Siqi: please give brief description of algorithm here.
*/

EP_STAT
gcl_index_find_cache(gcl_handle_t *gclh, gcl_log_index **out)
{
	EP_STAT estat = EP_STAT_OK;
	pthread_rwlock_rdlock(&table_lock);
	*out = ep_hash_search(name_index_table, sizeof(gcl_name_t), gclh->gcl_name);
	pthread_rwlock_unlock(&table_lock);

	return estat;
}

EP_STAT
gcl_index_create_cache(gcl_handle_t *gclh, gcl_log_index **out)
{
	EP_STAT estat = EP_STAT_OK;

	pthread_rwlock_wrlock(&table_lock);
	estat = gcl_log_index_new(gclh, out);
	EP_STAT_CHECK(estat, goto fail0);
	ep_hash_insert(name_index_table, sizeof(gcl_name_t), gclh->gcl_name, *out);
	gclh->log_index = *out;
fail0:
	pthread_rwlock_unlock(&table_lock);
	return estat;
}

EP_STAT
gcl_index_cache_get(gcl_log_index *entry, int64_t recno, int64_t *out)
{
	EP_STAT estat = EP_STAT_OK;

	pthread_rwlock_rdlock(&entry->lock);
	LONG_LONG_PAIR *found = circular_buffer_search(entry->index_cache, recno);
	if (found != NULL)
	{
		*out = found->value;
	}
	else
	{
		*out = LLONG_MAX;
	}
	pthread_rwlock_unlock(&entry->lock);

	return estat;
}

EP_STAT
gcl_index_cache_put(gcl_log_index *entry, int64_t recno, int64_t offset)
{
	EP_STAT estat = EP_STAT_OK;
	LONG_LONG_PAIR new_pair;

	new_pair.key = recno;
	new_pair.value = offset;

	circular_buffer_append(entry->index_cache, new_pair);

	return estat;
}


/*
**  FSIZEOF --- return the size of a file
*/

static off_t
fsizeof(FILE *fp)
{
	struct stat st;

	if (fstat(fileno(fp), &st) < 0)
	{
		ep_dbg_cprintf(Dbg, 1, "gcl_read: fstat failure: %s\n",
				strerror(errno));
		return -1;
	}

	return st.st_size;
}

/*
**	GCL_READ --- read a message from a gcl
**
**		Reads in a message indicated by msg->recno into msg.
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

#define SIZEOF_INDEX_HEADER		0	//XXX no header yet, but there should be

EP_STAT
gcl_read(gcl_handle_t *gclh,
		gdp_msg_t *msg)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_log_index *entry = gclh->log_index;
	LONG_LONG_PAIR *long_pair;
	int64_t offset = LLONG_MAX;

	EP_ASSERT_POINTER_VALID(gclh);

	ep_dbg_cprintf(Dbg, 14, "gcl_read(%" PRIgdp_recno "): ", msg->recno);

	pthread_rwlock_rdlock(&entry->lock);

	// first check if recno is in the index
	long_pair = circular_buffer_search(entry->index_cache, msg->recno);
	if (long_pair == NULL)
	{
		// recno is not in the index
		// now binary search through the disk index
		flockfile(entry->fp);

		off_t file_size = fsizeof(entry->fp);
		if (file_size < 0)
		{
			estat = ep_stat_from_errno(errno);
			gdp_log(estat, "gcl_read: ftell failed");
			goto fail1;
		}
		int64_t record_count = (file_size - SIZEOF_INDEX_HEADER) /
								sizeof(gcl_index_record);
		gcl_index_record index_record;
		size_t start = 0;
		size_t end = record_count;
		size_t mid;

		ep_dbg_cprintf(Dbg, 14,
				"searching index, rec_count=%" PRId64 ", file_size=%llu\n",
				record_count, file_size);
		if (file_size <= sizeof (gcl_log_header))
		{
			estat = GDP_STAT_CORRUPT_INDEX;
			goto fail1;
		}

		while (start < end)
		{
			mid = start + (end - start) / 2;
			ep_dbg_cprintf(Dbg, 47, "\t%zu ... %zu ... %zu\n", start, mid, end);
			fseek(entry->fp, mid * sizeof(gcl_index_record), SEEK_SET);
			fread(&index_record, sizeof(gcl_index_record), 1, entry->fp);
			if (msg->recno < index_record.recno)
			{
				end = mid;
			}
			else if (msg->recno > index_record.recno)
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
		ep_dbg_cprintf(Dbg, 14, "found in memory at %lld\n", offset);
	}

	if (offset == LLONG_MAX) // didn't find message
	{
		estat = EP_STAT_END_OF_FILE;
		goto fail0;
	}

	char read_buffer[GCL_READ_BUFFER_SIZE];
	gcl_log_record log_record;

	// read header
	flockfile(gclh->fp);
	fseek(gclh->fp, offset, SEEK_SET);
	fread(&log_record, sizeof(log_record), 1, gclh->fp);
	offset += sizeof(log_record);
	memcpy(&msg->ts, &log_record.timestamp, sizeof msg->ts);

	// read data in chunks and add it to the evbuffer
	int64_t data_length = log_record.data_length;
	while (data_length >= sizeof(read_buffer))
	{
		fread(&read_buffer, sizeof(read_buffer), 1, gclh->fp);
		gdp_buf_write(msg->dbuf, &read_buffer, sizeof(read_buffer));
		data_length -= sizeof(read_buffer);
	}
	if (data_length > 0)
	{
		fread(&read_buffer, data_length, 1, gclh->fp);
		gdp_buf_write(msg->dbuf, &read_buffer, data_length);
	}

	// done

fail1:
	funlockfile(gclh->fp);
fail0:
	pthread_rwlock_unlock(&entry->lock);

	return estat;
}

EP_STAT
gcl_create(gcl_name_t gcl_name,
		gcl_handle_t **pgclh)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_handle_t *gclh = NULL;
	FILE *data_fp;
	FILE *index_fp;

	// allocate a name
	if (gdp_gcl_name_is_zero(gcl_name))
	{
		_gdp_gcl_newname(gcl_name);
	}

	// allocate the memory to hold the GCL Handle
	// Note that ep_mem_* always returns, hence no test here
	estat = _gdp_gcl_newhandle(gcl_name, &gclh);
	EP_STAT_CHECK(estat, goto fail0);

	// create a file node representing the gcl
	{
		int data_fd;
		char data_pbuf[GCL_PATH_MAX];

		estat = get_gcl_path(gclh, data_pbuf, sizeof data_pbuf);
		EP_STAT_CHECK(estat, goto fail1);
		strlcat(data_pbuf, GCL_DATA_SUFFIX, sizeof(data_pbuf));

		data_fd = open(data_pbuf, O_RDWR | O_CREAT | O_APPEND | O_EXCL, 0644);
		if (data_fd < 0 ||
			(flock(data_fd, LOCK_EX) < 0))
		{
			estat = ep_stat_from_errno(errno);
			gdp_log(estat, "gcl_create: cannot create %s: %s",
				data_pbuf, strerror(errno));
			goto fail1;
		}
		data_fp = fdopen(data_fd, "a+");
		if (data_fp == NULL)
		{
			estat = ep_stat_from_errno(errno);
			gdp_log(estat, "gcl_create: cannot fdopen %s: %s",
				data_pbuf, strerror(errno));
			(void) close(data_fd);
			goto fail1;
		}
	}

	// create an offset index for that gcl
	{
		int index_fd;
		char index_pbuf[GCL_PATH_MAX];

		estat = get_gcl_path(gclh, index_pbuf, sizeof index_pbuf);
		EP_STAT_CHECK(estat, goto fail2);
		strlcat(index_pbuf, GCL_INDEX_SUFFIX, sizeof(index_pbuf));

		index_fd = open(index_pbuf, O_RDWR | O_CREAT | O_APPEND | O_EXCL, 0644);
		if (index_fd < 0)
		{
			estat = ep_stat_from_errno(errno);
			gdp_log(estat, "gcl_create: cannot create %s: %s",
				index_pbuf, strerror(errno));
			goto fail2;
		}
		index_fp = fdopen(index_fd, "a+");
		if (index_fp == NULL)
		{
			estat = ep_stat_from_errno(errno);
			gdp_log(estat, "gcl_create: cannot fdopen %s: %s",
				index_pbuf, strerror(errno));
			(void) close(index_fd);
			goto fail2;
		}
	}

	// write the header
	gcl_log_header log_header;
	int16_t metadata_size = 0; // XXX: compute size of metadata
	log_header.magic = GCL_LOG_MAGIC;
	log_header.version = GCL_VERSION;
	log_header.header_size = sizeof(gcl_log_header) + metadata_size;
	log_header.log_type = 0; // XXX: define different log types
	fwrite(&log_header, sizeof(log_header), 1, data_fp);
	// XXX: write metadata

	// TODO: will probably need creation date or some such later

	gclh->ver = log_header.version;
	gclh->data_offset = log_header.header_size;

	gcl_log_index *new_index = NULL;
	estat = gcl_index_create_cache(gclh, &new_index);
	EP_STAT_CHECK(estat, goto fail3);

	// success!
	fflush(data_fp);
	flock(fileno(data_fp), LOCK_UN);
	new_index->fp = index_fp;
	gclh->fp = data_fp;
	gclh->log_index = new_index;
	*pgclh = gclh;
	if (ep_dbg_test(Dbg, 10))
	{
		gcl_pname_t pname;

		gdp_gcl_printable_name(gclh->gcl_name, pname);
		ep_dbg_printf("Created GCL Handle %s\n", pname);
	}
	return estat;

fail3:
	fclose(index_fp);
fail2:
	fclose(data_fp);
fail1:
	ep_mem_free(gclh);
fail0:
	if (ep_dbg_test(Dbg, 8))
	{
		char ebuf[100];

		ep_dbg_printf("Could not create GCL Handle: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	GDP_GCL_OPEN --- open a gcl for reading or further appending
**
**		XXX: Should really specify whether we want to start reading:
**		(a) At the beginning of the log (easy).	 This includes random
**			access.
**		(b) Anything new that comes into the log after it is opened.
**			To do this we need to read the existing log to find the end.
*/

EP_STAT
gcl_open(gcl_name_t gcl_name,
			gdp_iomode_t mode,
			gcl_handle_t **pgclh)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_handle_t *gclh = NULL;
	FILE *data_fp;
	FILE *index_fp;
	char data_pbuf[GCL_PATH_MAX];
	char index_pbuf[GCL_PATH_MAX];

	estat = _gdp_gcl_newhandle(gcl_name, &gclh);
	EP_STAT_CHECK(estat, goto fail1);
	gclh->iomode = mode;

	const char *openmode = "a+";

	estat = get_gcl_path(gclh, data_pbuf, sizeof data_pbuf);
	EP_STAT_CHECK(estat, goto fail0);
	strlcat(data_pbuf, GCL_DATA_SUFFIX, sizeof(data_pbuf));

	estat = get_gcl_path(gclh, index_pbuf, sizeof index_pbuf);
	EP_STAT_CHECK(estat, goto fail0);
	strlcat(index_pbuf, GCL_INDEX_SUFFIX, sizeof(index_pbuf));

	if ((data_fp = fopen(data_pbuf, openmode)) == NULL)
	{
		estat = ep_stat_from_errno(errno);
		goto fail1;
	}

	gcl_log_header log_header;

	flockfile(data_fp);
	fseek(data_fp, 0, SEEK_SET);
	fread(&log_header, sizeof(log_header), 1, data_fp);
	funlockfile(data_fp);

	// XXX: read metadata entries
	if (log_header.magic != GCL_LOG_MAGIC)
	{
		fprintf(stderr, "gcl_open: magic mismatch - found: %" PRIu64 "x,"
				" expected: %" PRIu64 "x\n",
				log_header.magic, GCL_LOG_MAGIC);
		goto fail3;
	}

	if ((index_fp = fopen(index_pbuf, openmode)) == NULL)
	{
		estat = ep_stat_from_errno(errno);
		goto fail4;
	}

	gclh->ver = log_header.version;
	gclh->data_offset = log_header.header_size;

	gcl_log_index* index;
	gcl_index_find_cache(gclh, &index);
	if (index == NULL)
	{
		estat = gcl_index_create_cache(gclh, &index);
		EP_STAT_CHECK(estat, goto fail5);
	}

	gclh->fp = data_fp;
	gclh->log_index = index;

	index->fp = index_fp;

	*pgclh = gclh;

	return estat;

fail5:

fail4:

fail3:
	fclose(data_fp);
fail1:
fail0:
	if (gclh == NULL)
		ep_mem_free(gclh);

	{
		char ebuf[100];

		//XXX should log
		fprintf(stderr, "gcl_open: Couldn't open gcl: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	if (ep_dbg_test(Dbg, 10))
	{
		gcl_pname_t pname;
		char ebuf[100];

		gdp_gcl_printable_name(gcl_name, pname);
		ep_dbg_printf("Couldn't open gcl %s: %s\n",
				pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}

/*
**	GDP_GCL_CLOSE --- close an open gcl
*/

EP_STAT
gcl_close(gcl_handle_t *gclh)
{
	EP_STAT estat = EP_STAT_OK;

	EP_ASSERT_POINTER_VALID(gclh);
	if (gclh->fp == NULL)
	{
		estat = EP_STAT_ERROR;
		gdp_log(estat, "gcl_close: null fp");
	}
	else
	{
		fclose(gclh->fp);
	}

	// XXX: when do we remove the index cache for optimal performance?
	_gdp_gcl_cache_drop(gclh->gcl_name, 0);
	ep_mem_free(gclh);

	return estat;
}

/*
**	GDP_GCL_APPEND --- append a message to a writable gcl
*/

EP_STAT
gcl_append(gcl_handle_t *gclh,
			gdp_msg_t *msg)
{
	gcl_log_record log_record;
	gcl_index_record index_record;
	size_t dlen = gdp_buf_getlength(msg->dbuf);
	int64_t record_size = sizeof(gcl_log_record) + dlen;
	gcl_log_index *entry = (gcl_log_index *)(gclh->log_index);

	pthread_rwlock_wrlock(&entry->lock);

	log_record.recno = entry->max_recno + 1;
	log_record.timestamp = msg->ts;
	log_record.data_length = dlen;

	// write log record header
	fwrite(&log_record, sizeof(log_record), 1, gclh->fp);

	// write log record data
	while (dlen > 0)
	{
		char buf[1024];

		if (dlen > sizeof buf)
			dlen = sizeof buf;

		gdp_buf_read(msg->dbuf, buf, dlen);
		fwrite(buf, dlen, 1, gclh->fp);
		dlen = gdp_buf_getlength(msg->dbuf);
	}

	index_record.recno = log_record.recno;
	index_record.offset = entry->max_data_offset;

	// write index record
	fwrite(&index_record, sizeof(index_record), 1, entry->fp);

	// commit
	fflush(gclh->fp);
	fflush(entry->fp);
	gcl_index_cache_put(entry, index_record.recno, index_record.offset);
	++entry->max_recno;
	entry->max_index_offset += sizeof(index_record);
	entry->max_data_offset += record_size;

	pthread_rwlock_unlock(&entry->lock);

	msg->recno = log_record.recno;
	return EP_STAT_OK;
}

#if 0

/*
**	GDP_GCL_SUBSCRIBE --- subscribe to a gcl
*/

struct gdp_cb_arg
{
	struct event			*event;		// event triggering this callback
	gcl_handle_t			*gclh;		// GCL Handle triggering this callback
	gdp_gcl_sub_cbfunc_t	cbfunc;		// function to call
	void					*cbarg;		// argument to pass to cbfunc
	void					*buf;		// space to put the message
	size_t					bufsiz;		// size of buf
};


static void
gcl_sub_event_cb(evutil_socket_t fd,
		short what,
		void *cbarg)
{
	gdp_msg_t msg;
	EP_STAT estat;
	struct gdp_cb_arg *cba = cbarg;

	// get the next message from this GCL Handle
	estat = gcl_read(cba->gclh, GCL_NEXT_MSG, &msg, cba->gclh->revb);
	if (EP_STAT_ISOK(estat))
		(*cba->cbfunc)(cba->gclh, &msg, cba->cbarg);
	//XXX;
}


EP_STAT
gcl_subscribe(gcl_handle_t *gclh,
		gdp_gcl_sub_cbfunc_t cbfunc,
		long offset,
		void *buf,
		size_t bufsiz,
		void *cbarg)
{
	EP_STAT estat = EP_STAT_OK;
	struct gdp_cb_arg *cba;
	struct timeval timeout = { 0, 100000 };		// 100 msec

	EP_ASSERT_POINTER_VALID(gclh);
	EP_ASSERT_POINTER_VALID(cbfunc);

	cba = ep_mem_zalloc(sizeof *cba);
	cba->gclh = gclh;
	cba->cbfunc = cbfunc;
	cba->buf = buf;
	cba->bufsiz = bufsiz;
	cba->cbarg = cbarg;
	cba->event = event_new(GdpEventBase, fileno(gclh->fp),
			EV_READ|EV_PERSIST, &gcl_sub_event_cb, cba);
	event_add(cba->event, &timeout);
	//XXX;
	return estat;
}

EP_STAT
gcl_unsubscribe(gcl_handle_t *gclh,
		void (*cbfunc)(gcl_handle_t *, void *),
		void *arg)
{
	EP_ASSERT_POINTER_VALID(gclh);
	EP_ASSERT_POINTER_VALID(cbfunc);

	XXX;
}
#endif
