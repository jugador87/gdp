/* vim: set ai sw=4 sts=4 ts=4 :*/

#include "gdp.h"
#include "gdp_gclmd.h"

#include <event2/event.h>

#include <ep/ep_crypto.h>
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
_gdp_newname(gdp_name_t gname, gdp_gclmd_t *gmd)
{
	if (gmd == NULL)
	{
		// last resort: use random bytes
		evutil_secure_rng_get_bytes(gname, sizeof (gdp_name_t));
	}
	else
	{
		gdp_buf_t *gbuf = gdp_buf_new();
		size_t glen = _gdp_gclmd_serialize(gmd, gbuf);

		ep_crypto_md_sha256(gdp_buf_getptr(gbuf, glen), glen, gname);
		gdp_buf_free(gbuf);
	}
}
