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

#define GCL_LDF_MAGIC		UINT32_C(0x47434C31)	// 'GCL1'
#define GCL_LDF_VERSION		UINT32_C(20151001)		// on-disk version
#define GCL_LDF_MINVERS		UINT32_C(20151001)		// lowest readable version
#define GCL_LDF_MAXVERS		UINT32_C(20151001)		// highest readable version
#define GCL_LDF_SUFFIX		".gdplog"

#define GCL_LXF_MAGIC		UINT32_C(0x47434C78)	// 'GCLx'
#define GCL_LXF_VERSION		UINT32_C(20160101)		// on-disk version
#define GCL_LXF_MINVERS		UINT32_C(20160101)		// lowest readable version
#define GCL_LXF_MAXVERS		UINT32_C(20160101)		// highest readable version
#define GCL_LXF_SUFFIX		".gdpndx"

#define GCL_READ_BUFFER_SIZE 4096


/*
**  On-disk data format
**
**		Data in logs are stored in individual extent files, each of which
**		stores a contiguous series of records.  These definitions really
**		apply to individual extents, not the log as a whole.
**
**		TODO:	Is the metadata copied in each extent, or is it just in
**		TODO:	extent 0?  Should there be a single file representing
**		TODO:	the log as a whole, but contains no data, only metadata,
**		TODO:	kind of like a superblock?
**
**		TODO:	It probably makes sense to have a cache file that stores
**		TODO:	dynamic data about a log (e.g., how many records it has).
**		TODO:	Since this file is written a lot, it is important that it
**		TODO:	can be reconstructed.
**
**		Each extent has a fixed length extent header (called here a log
**		header), followed by the log metadata, followed by a series of
**		data records.  Each record has a header followed by data.
**
**		The GCL metadata consists of num_metadata_entries (N)
**		descriptors, each of which has a uint32_t "name" and a
**		uint32_t length.  In other words, the descriptors are an
**		an array of names and lengths: (n1, l1, n2, l2 ... nN, lN).
**		The descriptors are followed by the actual metadata content.
**
**		Some of the metadata names are reserved for internal use (e.g.,
**		storage of the public key associated with the log), but
**		other metadata names can be added that will be interpreted
**		by applications.
*/

// an individual record
typedef struct gcl_log_record
{
	gdp_recno_t		recno;
	EP_TIME_SPEC	timestamp;
	uint16_t		sigmeta;			// signature metadata (size & hash alg)
	int16_t			reserved1;			// reserved for future use
	int32_t			reserved2;			// reserved for future use
	int64_t			data_length;
	char			data[0];
										// signature is after the data
} gcl_log_record_t;


// a data file header
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
} gcl_log_header_t;


/*
**  On-disk index record format.
*/

typedef struct gcl_index_entry
{
	gdp_recno_t	recno;			// record number
	int64_t		offset;			// offset into extent file
	uint32_t	extent;			// id of extent (for segmented logs)
} gcl_index_entry_t;

typedef struct gcl_index_header
{
	uint32_t	magic;
	uint32_t	version;
	uint32_t	header_size;
	uint32_t	reserved1;
	gdp_recno_t	first_recno;
} gcl_index_header_t;

// return maximum record number for a given GCL
extern gdp_recno_t	gcl_max_recno(gdp_gcl_t *gcl);

#define SIZEOF_INDEX_HEADER		(sizeof(gcl_index_header_t))
#define SIZEOF_INDEX_RECORD		(sizeof(gcl_index_entry_t))

#endif //_GDPLOGD_PHYSLOG_H_
