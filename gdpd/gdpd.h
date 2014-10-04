/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	Headers for the GDP Daemon
*/

#ifndef _GDPD_H_
#define _GDPD_H_		1

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_stat.h>

#include <gdp/gdp.h>
#include <gdp/gdp_log.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_pkt.h>
#include <gdp/gdp_stat.h>

#include <unistd.h>
#include <string.h>
#include <sys/queue.h>

/*
**  Private GCL definitions for gdpd only
**
**		The gcl field is because the LIST macros don't understand
**		having the links in a substructure (i.e., I can't link a
**		gdp_gcl_xtra to a gdp_gcl).
*/

struct gdp_gcl_xtra
{
	// declarations relating to semantics
	gdp_gcl_t			*gcl;			// enclosing GCL
	time_t				utime;			// last usage time (sec only)
	LIST_ENTRY(gdp_gcl_xtra)
						ulist;			// list sorted by use time

	// physical implementation declarations
	long				ver;			// version number of on-disk file
	FILE				*fp;			// pointer to the on-disk file
	struct index_entry	*log_index;		// ???
	off_t				data_offset;	// offset for start of data
};


/*
**  Definitions for the gdpd-specific GCL handling
*/

extern EP_STAT	gcl_alloc(				// allocate a new GCL
					gcl_name_t gcl_name,
					gdp_iomode_t iomode,
					gdp_gcl_t **pgclh);

extern EP_STAT	gcl_open(				// open an existing physical GCL
					gcl_name_t gcl_name,
					gdp_iomode_t iomode,
					gdp_gcl_t **pgclh);

extern void		gcl_close(				// close an open GCL
					gdp_gcl_t *gclh);

extern void		gcl_touch(				// make a GCL recently used
					gdp_gcl_t *gclh);

extern void		gcl_showusage(			// show GCL LRU list
					FILE *fp);

extern EP_STAT	get_open_handle(		// get open handle (pref from cache)
					gdp_req_t *req,
					gdp_iomode_t iomode);

#endif //_GDPD_H_
