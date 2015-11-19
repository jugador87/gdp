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

#define GCL_DIR				"/var/swarm/gdp/gcls"

#define GCL_LOG_MAGIC		UINT32_C(0x47434C31)	// 'GCL1'
#define GCL_LOG_VERSION		UINT32_C(20151001)		// on-disk version
#define GCL_LOG_MINVERS		UINT32_C(20151001)		// lowest readable version
#define GCL_LOG_MAXVERS		UINT32_C(20151001)		// highest readable version

#define GCL_DATA_SUFFIX		".gdplog"
#define GCL_INDEX_SUFFIX	".gdpndx"

#define GCL_READ_BUFFER_SIZE 4096


/*
**  On-disk per-record format
*/

typedef struct gcl_log_record
{
	gdp_recno_t		recno;
	EP_TIME_SPEC	timestamp;
	uint16_t		sigmeta;			// signature metadata (size & hash alg)
	int16_t			reserved1;			// reserved for future use
	int32_t			reserved2;			// reserved for future use
	int64_t			data_length;
	char			data[];
										// signature is after the data
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
	uint32_t	magic;
	uint32_t	version;
	uint32_t	header_size; 	// the total size of the header such that
								// the data records begin at offset header_size
	uint32_t	reserved1;		// reserved for future use
	uint16_t	num_metadata_entries;
	uint16_t	log_type;		// directory, indirect, data, etc. (unused)
	uint32_t	extent;			// extent number
	uint64_t	reserved2;		// reserved for future use
	gdp_name_t	gname;			// the name of this log
	gdp_recno_t	recno_offset;	// record number offset (first stored recno - 1)
} gcl_log_header;


/*
**  On-disk index record format.
*/

typedef struct gcl_index_record
{
	gdp_recno_t	recno;			// record number
	int64_t		offset;			// offset into extent file
	uint32_t	extent;			// id of extent (for segmented logs)
} gcl_index_record;

// return maximum record number for a given GCL
extern gdp_recno_t	gcl_max_recno(gdp_gcl_t *gcl);

#define SIZEOF_INDEX_HEADER		0	//XXX no header yet, but there should be
#define SIZEOF_INDEX_RECORD		(sizeof(gcl_index_record))

#endif //_GDPLOGD_PHYSLOG_H_
