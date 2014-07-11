/* vim: set ai sw=4 sts=4 : */

/*
**  Headers for the GDP Daemon
*/

#define GDP_MSG_EXTRA \
	    off_t offset;

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_stat.h>

#include <gdp/gdp.h>
#include <gdp/gdp_log.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_stat.h>

#include <unistd.h>
#include <string.h>
