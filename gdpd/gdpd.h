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

/*
**  Private GCL definitions for gdpd only
*/

struct gdp_gcl_xtra
{
	long				ver;			// version number of on-disk file
	FILE				*fp;			// pointer to the on-disk file
	struct index_entry	*log_index;		// ???
	off_t				data_offset;	// offset for start of data
};

#endif //_GDPD_H_
