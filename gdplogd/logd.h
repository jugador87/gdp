/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	Headers for the GDP Log Daemon
*/

#ifndef _GDPLOGD_H_
#define _GDPLOGD_H_		1

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_stat.h>

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_pdu.h>
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

	// physical implementation declarations
	long				ver;			// version number of on-disk file
	FILE				*fp;			// pointer to the on-disk file
	struct index_entry	*log_index;		// ???
	off_t				data_offset;	// offset for start of data
	uint16_t			nmetadata;		// number of metadata entries
	uint16_t			log_type;		// from log header
};


/*
**  Definitions for the gdpd-specific GCL handling
*/

extern EP_STAT	gcl_alloc(				// allocate a new GCL
					gdp_name_t gcl_name,
					gdp_iomode_t iomode,
					gdp_gcl_t **pgcl);

extern EP_STAT	gcl_open(				// open an existing physical GCL
					gdp_name_t gcl_name,
					gdp_iomode_t iomode,
					gdp_gcl_t **pgcl);

extern void		gcl_close(				// close an open GCL
					gdp_gcl_t *gcl);

extern void		gcl_touch(				// make a GCL recently used
					gdp_gcl_t *gcl);

extern void		gcl_showusage(			// show GCL LRU list
					FILE *fp);

extern EP_STAT	get_open_handle(		// get open handle (pref from cache)
					gdp_req_t *req,
					gdp_iomode_t iomode);

extern void		gcl_reclaim_resources(void);	// reclaim old GCLs


/*
**  Definitions for the protocol module
*/

extern EP_STAT	gdpd_proto_init(void);	// initialize protocol module

extern EP_STAT	dispatch_cmd(			// dispatch a request
					gdp_req_t *req);


/*
**  Advertisements
*/

extern EP_STAT	logd_advertise_all(int cmd);

extern void		logd_advertise_one(gdp_name_t name);

extern void		sub_send_message_notification(
					gdp_req_t *req,
					gdp_datum_t *datum,
					int cmd);

#endif //_GDPLOGD_H_
