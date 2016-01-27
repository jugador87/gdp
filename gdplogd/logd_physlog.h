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
**		This module is private to the physical layer, but it is
**			used by apps/log-view since that needs to crack the
**			physical layout.
*/

// default directory for GCL storage
#define GCL_DIR				"/var/swarm/gdp/gcls"

// magic numbers and versions for on-disk structures
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

#define GCL_READ_BUFFER_SIZE 4096			// size of I/O buffers


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
**		The GCL metadata consists of n_md_entries (N)
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

// an individual record in an extent
typedef struct
{
	gdp_recno_t		recno;
	EP_TIME_SPEC	timestamp;
	uint16_t		sigmeta;			// signature metadata (size & hash alg)
	int16_t			reserved1;			// reserved for future use
	int32_t			reserved2;			// reserved for future use
	int64_t			data_length;
	char			data[0];
										// signature is after the data
} extent_record_t;


// a data extent file header
typedef struct
{
	uint32_t	magic;			// GCL_LDF_MAGIC
	uint32_t	version;		// GCL_LDF_VERSION
	uint32_t	header_size; 	// the total size of the header such that
								// the data records begin at offset header_size
	uint32_t	reserved1;		// reserved for future use
	uint16_t	n_md_entries;	// number of metadata entries
	uint16_t	log_type;		// directory, indirect, data, etc. (unused)
	uint32_t	extent;			// extent number
	uint64_t	reserved2;		// reserved for future use
	gdp_name_t	gname;			// the name of this log
	gdp_recno_t	recno_offset;	// first recno stored in this extent - 1
} extent_header_t;


/*
**  In-Memory representation of Per-Extent info
**
**		This only includes the information that may (or does) vary
**		on a per-extent basis.  For example, different extents might
**		have different versions or header sizes, but not different
**		metadata, which must be fixed per log (even though on disk
**		that information is actually replicated in each extent.
*/

typedef struct
{
	FILE				*fp;				// file pointer to extent
	uint32_t			ver;				// on-disk file version
	uint32_t			extno;				// extent number
	size_t				header_size;		// size of extent file hdr
	gdp_recno_t			recno_offset;		// first recno in extent - 1
	off_t				max_offset;			// size of extent file
	EP_TIME_SPEC		retain_until;		// retain at least until this date
	EP_TIME_SPEC		remove_by;			// must be gone by this date
} extent_t;


/*
**  On-disk index record format
**
**		Currently the index consists of (essentially) an array of fixed
**		size entries.  The index header contains the record offset (i.e.,
**		the lowest record number that can be accessed).
**
**		The index covers the entirety of the log (i.e., not just one
**		extent), so any records numbered below that first record number
**		are inaccessible at any location.  Each index entry contains the
**		extent number in which the record occurs and the offset into that
**		extent file.  In theory records could be interleaved between
**		extents, but that's not how things work now.
**
**		At some point it may be that the local server doesn't have all
**		the extents for a given log.  Figuring out the physical location
**		of an extent from the index is not addressed here.
**		(The current implementation assumes that all extents are local.)
**
**		The index is not intended to have unique information.  Given the
**		set of extent files, it should be possible to rebuild the index.
*/

typedef struct index_entry
{
	gdp_recno_t	recno;			// record number
	int64_t		offset;			// offset into extent file
	uint32_t	extent;			// id of extent (for segmented logs)
	uint32_t	reserved;		// make padding explicit
} index_entry_t;

typedef struct index_header
{
	uint32_t	magic;			// GCL_LDX_MAGIC
	uint32_t	version;		// GCL_LDX_VERSION
	uint32_t	header_size;	// offset to first index entry
	uint32_t	reserved1;		// must be zero
	gdp_recno_t	min_recno;		// the first record number in the log
} index_header_t;

#define SIZEOF_INDEX_HEADER		(sizeof(index_header_t))
#define SIZEOF_INDEX_RECORD		(sizeof(index_entry_t))


/*
**  The in-memory cache of the physical index data.
**
**		This doesn't necessarily cover the entire index if the index
**		gets large.
**
**		This is currently a circular buffer, but that's probably
**		not the best representation since we could potentially have
**		multiple readers accessing wildly different parts of the
**		cache.  Applications trying to do historic summaries will
**		be particularly problematic.
*/

//typedef struct
//{
//	size_t				max_size;			// number of entries in data
//	size_t				current_size;		// number of filled entries in data
//	index_entry_t		data[];
//} index_cache_t;


/*
**  The in-memory representation of the on-disk log index.
*/

struct phys_index
{
	// information about on-disk format
	FILE				*fp;					// recno -> offset file handle
	int64_t				max_offset;				// size of index file
	size_t				header_size;			// size of hdr in index file

	// a cache of the contents
//	index_cache_t		cache;					// in-memory cache
};


/*
**  Per-log info.
**
**		There is no single instantiation of a log, so this is really
**		a representation of an abstraction.  It includes information
**		about all extents and the index.
*/

struct physinfo
{
	// reading and writing to the log requires holding this lock
	EP_THR_RWLOCK		lock;

	// info regarding the entire log (not extent)
	gdp_recno_t			min_recno;				// first recno in log
	gdp_recno_t			max_recno;				// last recno in log (dynamic)

	// info regarding the extent files
	uint32_t			nextents;				// number of extents
	uint32_t			last_extent;			// extent being written
	extent_t			**extents;				// list of extent pointers
												// can be dynamically expanded

	// info regarding the index file
	struct phys_index	index;
};

#endif //_GDPLOGD_PHYSLOG_H_
