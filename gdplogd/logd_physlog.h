/* vim: set ai sw=4 sts=4 ts=4 : */

#ifndef _GDPLOGD_PHYSLOG_H_
#define _GDPLOGD_PHYSLOG_H_		1

#include "logd.h"

/*
**	Headers for the physical log implementation.
**		This is how bytes are actually laid out on the disk.
*/

EP_STAT			gcl_physlog_init();

EP_STAT			gcl_offset_cache_init(
						gdp_gcl_t *gcl);

EP_STAT			gcl_physread(
						gdp_gcl_t *gcl,
						gdp_datum_t *datum);

EP_STAT			gcl_physcreate(
						gdp_gcl_t *pgcl,
						gdp_gclmd_t *gmd);

EP_STAT			gcl_physopen(
						gdp_gcl_t *gcl);

EP_STAT			gcl_physclose(
						gdp_gcl_t *gcl);

EP_STAT			gcl_physappend(
						gdp_gcl_t *gcl,
						gdp_datum_t *datum);

EP_STAT			gcl_physgetmetadata(
						gdp_gcl_t *gcl,
						gdp_gclmd_t **gmdp);

void			gcl_physforeach(
						void (*func)(
							gdp_name_t name,
							void *ctx),
						void *ctx);

#define GCL_DIR				"/var/swarm/gcls"

#define GCL_LOG_MAGIC		UINT32_C(0x07434C31)	// 'GCL1'
#define GCL_LOG_VERSION		UINT32_C(0)
#define GCL_LOG_MINVERS		UINT32_C(0)			// lowest version we can read
#define GCL_LOG_MAXVERS		UINT32_C(0)			// highest version we can read

#define GCL_DATA_SUFFIX		".data"
#define GCL_INDEX_SUFFIX	".index"

#define GCL_READ_BUFFER_SIZE 4096


/*
**  On-disk per-record format
*/

typedef struct gcl_log_record
{
	gdp_recno_t		recno;
	EP_TIME_SPEC	timestamp;
	int32_t			reserved1;			// reserved for future use
	int32_t			reserved2;			// reserved for future use
	int64_t			data_length;
	char			data[];
} gcl_log_record;

/*
**  On-disk per-log header
**
**		The GCL metadata consists of num_metadata_entires (N), followed by
**		N lengths (l_1, l_2, ... , l_N), followed by N **non-null-terminated**
**		strings of lengths l_1, ... , l_N. It is up to the user application to
**		interpret the metadata strings.
**
**		num_metadata_entries is a int16_t
**		l_1, ... , l_N are int16_t
*/

typedef struct gcl_log_header
{
	int32_t magic;
	int32_t version;
	int32_t header_size; 	// the total size of the header such that
							// the data records begin at offset header_size
	int16_t num_metadata_entries;
	int16_t log_type;		// directory, indirect, data, etc.
} gcl_log_header;


/*
**  On-disk index record format.
*/

typedef struct gcl_index_record
{
	gdp_recno_t recno;
	int64_t offset;
} gcl_index_record;

// return maximum record number for a given GCL
extern gdp_recno_t	gcl_max_recno(gdp_gcl_t *gcl);

#endif //_GDPLOGD_PHYSLOG_H_
