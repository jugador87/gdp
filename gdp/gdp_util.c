/* vim: set ai sw=4 sts=4 ts=4 :*/

#include "gdp.h"

#include <event2/event.h>

#include <ep/ep_dbg.h>

/*
**	GDP Utility routines
*/

//static EP_DBG	Dbg = EP_DBG_INIT("gdp.util", "GDP utility routines");


/*
**   _GDP_NEWNAME --- create a new GDP name
**
**		Really just creates a random number (for now).
*/

void
_gdp_newname(gdp_name_t gname)
{
	evutil_secure_rng_get_bytes(gname, sizeof (gdp_name_t));
}
