/* vim: set ai sw=4 sts=4 ts=4: */

/*
 **  Headers for the physical log implementation.
 **	This is how bytes are actually laid out on the disk.
 */

EP_STAT
gcl_physlog_init();

EP_STAT
gcl_offset_cache_init(gcl_handle_t *gclh);

EP_STAT
gcl_read(gcl_handle_t *gclh, long msgno, gdp_msg_t *msg, struct evbuffer *evb);

EP_STAT
gcl_create(gcl_name_t gcl_name, gcl_handle_t **pgclh);

EP_STAT
gcl_open(gcl_name_t gcl_name, gdp_iomode_t mode, gcl_handle_t **pgclh);

EP_STAT
gcl_close(gcl_handle_t *gclh);

EP_STAT
gcl_append(gcl_handle_t *gclh, gdp_msg_t *msg);

#define GCL_MAX_LINE		128	// max length of text line in gcl
#define GCL_PATH_LEN		200	// max length of pathname
#define GCL_DIR			"/var/tmp/gcl"
#define GCL_VERSION		0
#define GCL_MAGIC		"#!GDP-GCL "
#define MSG_MAGIC		"\n#!MSG "

#define GCL_LOG_MAGIC 0x4041424344454647LL

#define GCL_DATA_SUFFIX		".data"
#define GCL_INDEX_SUFFIX	".index"

#define GCL_READ_BUFFER_SIZE 4096

typedef struct gcl_log_record
{
	long long msgno;
	tt_interval_t timestamp;
	long long data_length;
	char data[];
} gcl_log_record;

typedef struct gcl_log_header
{
	long long magic;
	long long version;
	gcl_log_record records[]; // rest of the file
} gcl_log_header;

typedef struct gcl_index_record
{
	long long msgno;
	long long offset;
} gcl_index_record;
