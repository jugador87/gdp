/* vim: set ai sw=4 sts=4 : */

#include "circular_buffer.h"
#include "gdpd.h"
#include "gdpd_gcl.h"
#include "gdpd_physlog.h"

#include <ep/ep_hash.h>
#include <ep/ep_mem.h>

#include <gdp/gdp_protocol.h>

#include <linux/limits.h>
#include <sys/file.h>
#include <pthread.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gdpd.physlog",
				"GDP Daemon Physical Log Implementation");

static const char *GCLDir = NULL;	// the gcl data directory
static EP_HASH *name_index_table = NULL;
// reading and writing to the EP_HASH mapping gcl_name -> index requires holding table_mutex
// EP_HASH is not thread-safe
static pthread_rwlock_t table_lock = PTHREAD_RWLOCK_INITIALIZER;

typedef struct index_entry {
	// reading and writing to the log requires holding this lock
	pthread_rwlock_t lock;
	int fd;
	FILE *fp;
	long long max_msgno;
	long long max_data_offset;
	long long max_index_offset;
	CIRCULAR_BUFFER *index_cache;
} index_entry;

EP_STAT
gcl_physlog_init() {
	EP_STAT estat = EP_STAT_OK;

	GCLDir = ep_adm_getstrparam("swarm.gdp.gcl.dir", GCL_DIR);
	name_index_table = ep_hash_new("name_index_table", NULL, sizeof(gcl_name_t));

	return estat;
}

static EP_STAT
index_entry_new(index_entry **out) {
	*out = malloc(sizeof(index_entry));
	if (*out == NULL) {
		return EP_STAT_ERROR;
	}
	if (pthread_rwlock_init(&(*out)->lock, NULL) != 0) {
		goto fail0;
	}
	(*out)->fd = -1;
	(*out)->fp = NULL;
	(*out)->max_msgno = 0;
	(*out)->max_data_offset = sizeof(gcl_log_header);
	(*out)->max_index_offset = 0;
	int cache_size = ep_adm_getintparam("swarm.gdp.index.cachesize", 2048);
	(*out)->index_cache = circular_buffer_new(cache_size);
	return EP_STAT_OK;

fail0:
	free(*out);
	*out = NULL;
	return EP_STAT_ERROR;
}

static EP_STAT
get_gcl_path(gcl_handle_t *gclh, char *pbuf, int pbufsiz)
{
    gcl_pname_t pname;
    int i;

    EP_ASSERT_POINTER_VALID(gclh);

    gdp_gcl_printable_name(gclh->gcl_name, pname);
    i = snprintf(pbuf, pbufsiz, "%s/%s", GCLDir, pname);
    if (i < pbufsiz)
    {
    	return EP_STAT_OK;
    }
    else
    {
    	return EP_STAT_ERROR;
    }
}

EP_STAT
gcl_index_find_cache(gcl_handle_t *gclh, index_entry **out)
{
	EP_STAT estat = EP_STAT_OK;
	pthread_rwlock_rdlock(&table_lock);
	*out = ep_hash_search(name_index_table, sizeof(gcl_name_t), gclh->gcl_name);
	pthread_rwlock_unlock(&table_lock);

	return estat;
}

EP_STAT
gcl_index_create_cache(gcl_handle_t *gclh, index_entry **out)
{
	EP_STAT estat = EP_STAT_OK;

	pthread_rwlock_wrlock(&table_lock);
	estat = index_entry_new(out);
	EP_STAT_CHECK(estat, goto fail0);
	ep_hash_insert(name_index_table, sizeof(gcl_name_t), gclh->gcl_name, *out);
	gclh->index_entry = *out;
fail0:
	pthread_rwlock_unlock(&table_lock);
	return estat;
}

EP_STAT
gcl_index_cache_get(index_entry *entry, long long msgno, long long *out)
{
	EP_STAT estat = EP_STAT_OK;

	pthread_rwlock_rdlock(&entry->lock);
	LONG_LONG_PAIR *found = circular_buffer_search(entry->index_cache, msgno);
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
gcl_index_cache_put(index_entry *entry, long long msgno, long long offset)
{
	EP_STAT estat = EP_STAT_OK;
	LONG_LONG_PAIR new_pair;

	new_pair.key = msgno;
	new_pair.value = offset;

	circular_buffer_append(entry->index_cache, new_pair);

	return estat;
}

EP_STAT
gcl_read(gcl_handle_t *gclh,
	long msgno,
	gdp_msg_t *msg,
	struct evbuffer *evb)
{
	EP_STAT estat = EP_STAT_OK;
	index_entry *entry = gclh->index_entry;
	LONG_LONG_PAIR *long_pair;
	long long offset = LLONG_MAX;

	pthread_rwlock_rdlock(&entry->lock);

	// first check if msgno is in the index
	long_pair = circular_buffer_search(entry->index_cache, msgno);
	if (long_pair == NULL) {
		// msgno is not in the index
		// now binary search through the disk index

		FILE *index_fp = fdopen(dup(entry->fd), "r");
		off_t file_size = fseek(index_fp, 0, SEEK_END);
		long long record_count = ((file_size - sizeof(gcl_log_header)) / sizeof(gcl_index_record));
		gcl_index_record index_record;
		size_t start = 0;
		size_t end = record_count;
		size_t mid;

		while (start < end)
		{
			mid = start + (end - start) / 2;
			fseek(index_fp, mid * sizeof(gcl_index_record), SEEK_SET);
			fread(&index_record, sizeof(gcl_index_record), 1, index_fp);
			if (msgno < index_record.msgno)
			{
				end = mid;
			}
			else if (msgno > index_record.msgno)
			{
				start = mid + 1;
			}
			else
			{
				offset = index_record.offset;
				break;
			}
		}

		offset = LLONG_MAX;
	}
	else
	{
		offset = long_pair->value;
	}

	if (offset == LLONG_MAX) // didn't find message
	{
		estat = EP_STAT_END_OF_FILE;
		goto fail0;
	}

	char read_buffer[GCL_READ_BUFFER_SIZE];
	gcl_log_record log_record;
	int new_fd = dup(gclh->fd);
	if (new_fd == -1)
	{
		fprintf(stderr, "%s", strerror(errno));
		goto fail0;
	}
	FILE *data_fp = fdopen(new_fd, "r");
	fseek(data_fp, offset, SEEK_SET);

	// read header
	fread(&log_record, sizeof(log_record), 1, data_fp);

	long long data_length = log_record.data_length;

	// read data in chunks and add it to the evbuffer
	while (data_length >= sizeof(read_buffer))
	{
		fread(&read_buffer, sizeof(read_buffer), 1, data_fp);
		evbuffer_add(evb, &read_buffer, sizeof(read_buffer));
		data_length -= sizeof(read_buffer);
	}
	if (data_length > 0)
	{
		fread(&read_buffer, data_length, 1, data_fp);
		evbuffer_add(evb, &read_buffer, data_length);
	}

	// done

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
	int data_fd = -1;
	int index_fd = -1;
	FILE *data_fp;
	FILE *index_fp;
	char data_pbuf[PATH_MAX];
	char index_pbuf[PATH_MAX];

	// allocate the memory to hold the GCL Handle
    // Note that ep_mem_* always returns, hence no test here
    estat = gdpd_gclh_new(&gclh);
    EP_STAT_CHECK(estat, goto fail0);

    // allocate a name
    if (gdp_gcl_name_is_zero(gcl_name))
    {
    	gdp_gcl_newname(gcl_name);
    }
    memcpy(gclh->gcl_name, gcl_name, sizeof gclh->gcl_name);

    // create a file node representing the gcl
    // XXX: this is where we start to seriously cheat
    estat = get_gcl_path(gclh, data_pbuf, sizeof data_pbuf);
    EP_STAT_CHECK(estat, goto fail1);
    strncat(data_pbuf, GCL_DATA_SUFFIX, sizeof(GCL_DATA_SUFFIX));

    estat = get_gcl_path(gclh, index_pbuf, sizeof index_pbuf);
    EP_STAT_CHECK(estat, goto fail2);
    strncat(index_pbuf, GCL_INDEX_SUFFIX, sizeof(GCL_INDEX_SUFFIX));

    if ((data_fd = open(data_pbuf, O_RDWR | O_CREAT | O_APPEND | O_EXCL, 0644)) < 0 ||
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
		goto fail1;
    }

    if ((index_fd = open(index_pbuf, O_RDWR | O_CREAT | O_APPEND | O_EXCL, 0644)) < 0)
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
    	goto fail3;
    }

    index_entry *new_entry = NULL;
    estat = gcl_index_create_cache(gclh, &new_entry);
    EP_STAT_CHECK(estat, goto fail3);

    // write the header metadata
    gcl_log_header log_header;
    log_header.magic = GCL_LOG_MAGIC;
    log_header.version = GCL_VERSION;
    fwrite(&log_header, sizeof(log_header), 1, data_fp);

    // TODO: will probably need creation date or some such later

    // success!
    fflush(data_fp);
    flock(data_fd, LOCK_UN);
    new_entry->fd = index_fd;
    new_entry->fp = index_fp;
    gclh->fd = data_fd;
    gclh->fp = data_fp;
    gclh->index_entry = new_entry;
    gclh->ver = log_header.version;
    *pgclh = gclh;
    if (ep_dbg_test(Dbg, 10))
    {
		gcl_pname_t pname;

		gdp_gcl_printable_name(gclh->gcl_name, pname);
		ep_dbg_printf("Created GCL Handle %s\n", pname);
    }
    return estat;

fail3:
fail2:
	if (index_fd >= 0)
	{
		close(index_fd);
	}

fail1:
    if (data_fd >= 0)
	    close(data_fd);
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

EP_STAT
gcl_open(gcl_name_t gcl_name,
	gdp_iomode_t mode,
	gcl_handle_t **pgclh)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_handle_t *gclh = NULL;
	int data_fd = -1;
	int index_fd = -1;
	FILE *data_fp;
    char data_pbuf[PATH_MAX];
	char index_pbuf[PATH_MAX];

    estat = gdpd_gclh_new(&gclh);
    EP_STAT_CHECK(estat, goto fail1);
    memcpy(gclh->gcl_name, gcl_name, sizeof gclh->gcl_name);
    gclh->iomode = mode;

	const char *openmode = "a+";

	estat = get_gcl_path(gclh, data_pbuf, sizeof data_pbuf);
	EP_STAT_CHECK(estat, goto fail0);
	strncat(data_pbuf, GCL_DATA_SUFFIX, sizeof(GCL_DATA_SUFFIX));

	estat = get_gcl_path(gclh, index_pbuf, sizeof index_pbuf);
	EP_STAT_CHECK(estat, goto fail0);
	strncat(index_pbuf, GCL_INDEX_SUFFIX, sizeof(GCL_INDEX_SUFFIX));

	if ((data_fd = open(data_pbuf, O_RDWR | O_APPEND, 0644)) < 0)
	{
		estat = ep_stat_from_errno(errno);
		goto fail1;
	}

	if ((data_fp = fdopen(data_fd, openmode)) == NULL) {
		estat = ep_stat_from_errno(errno);
		goto fail2;
	}

	gcl_log_header log_header;

	fread(&log_header, sizeof(log_header), 1, data_fp);

	if (log_header.magic != GCL_LOG_MAGIC) {
		fprintf(stderr, "gcl_open: magic mismatch - found: %lld, expected: %lld\n",
			log_header.magic, GCL_LOG_MAGIC);
		goto fail3;
	}

	if ((index_fd = open(index_pbuf, O_RDWR | O_APPEND, 0644)) < 0)
	{
		// XXX: recreate index instead of failing
		estat = ep_stat_from_errno(errno);
		goto fail4;
	}

	index_entry* entry;
	gcl_index_find_cache(gclh, &entry);
	if (entry == NULL)
	{
		estat = gcl_index_create_cache(gclh, &entry);
		EP_STAT_CHECK(estat, goto fail5);
	}

	gclh->fd = data_fd;
    gclh->fp = data_fp;
    gclh->index_entry = entry;
    gclh->ver = log_header.version;

    *pgclh = gclh;

    return estat;

fail5:

fail4:

fail3:
	fclose(data_fp);
fail2:
	close(data_fd);
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
    if (gclh->fd == -1)
    {
		estat = EP_STAT_ERROR;
		gdp_log(estat, "gcl_close: null fp");
    }
    else
    {
    	close(gclh->fd);
    }
    evbuffer_free(gclh->revb);
    ep_mem_free(gclh->offcache);
    // XXX: when do we remove the index cache for optimal performance?
    gdp_gcl_cache_drop(gclh->gcl_name, 0);
    ep_mem_free(gclh);

    return estat;
}

EP_STAT
gcl_append(gcl_handle_t *gclh,
	gdp_msg_t *msg)
{
	gcl_log_record log_record;
	gcl_index_record index_record;
	long long record_size = sizeof(gcl_log_record) + msg->len;
	index_entry *entry = (index_entry *)(gclh->index_entry);

	pthread_rwlock_wrlock(&entry->lock);

	log_record.msgno = entry->max_msgno + 1;
	log_record.timestamp = msg->ts;
	log_record.data_length = msg->len;

	// write log record header
	fwrite(&log_record, sizeof(log_record), 1, gclh->fp);

	// write log record data
	fwrite(msg->data, msg->len, 1, gclh->fp);

	index_record.msgno = log_record.msgno;
	index_record.offset = entry->max_data_offset;

	// write index record
	fwrite(&index_record, sizeof(index_record), 1, entry->fp);

	// commit
	gcl_index_cache_put(entry, index_record.msgno, index_record.offset);
	++entry->max_msgno;
	entry->max_index_offset += sizeof(index_record);
	entry->max_data_offset += record_size;

	pthread_rwlock_unlock(&entry->lock);

	return EP_STAT_OK;
}

