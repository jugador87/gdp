/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <gdp/gdp.h>
#include <gdp/gdp_log.h>
#include <gdp/gdp_stat.h>
#include <gdp/gdp_pkt.h>
#include <gdp/gdp_priv.h>
#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>
#include <ep/ep_prflags.h>
#include <ep/ep_string.h>
#include <ep/ep_thr.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/*
**	This implements GDP Connection Log (GCL) utilities.
*/

/************************  PRIVATE	************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl", "GCL utilities for GDP");



/***********************************************************************
**
**	GCL Caching
**		Let's us find the internal representation of the GCL from
**		the name.  These are not really intended for public use,
**		but they are shared with gdpd.
**
**		FIXME This is a very stupid implementation at the moment.
**
**		FIXME Makes no distinction between io modes (we cludge this
**			  by just opening everything for r/w for now)
***********************************************************************/

static EP_HASH		*OpenGCLCache;

EP_STAT
_gdp_gcl_cache_init(void)
{
	EP_STAT estat = EP_STAT_OK;

	if (OpenGCLCache == NULL)
	{
		OpenGCLCache = ep_hash_new("OpenGCLCache", NULL, 0);
		if (OpenGCLCache == NULL)
		{
			estat = ep_stat_from_errno(errno);
			gdp_log(estat, "gdp_gcl_cache_init: could not create OpenGCLCache");
			ep_app_abort("gdp_gcl_cache_init: could not create OpenGCLCache");
		}
	}
	return estat;
}


gcl_handle_t *
_gdp_gcl_cache_get(gcl_name_t gcl_name, gdp_iomode_t mode)
{
	gcl_handle_t *gclh;

	// see if we have a pointer to this GCL in the cache
	gclh = ep_hash_search(OpenGCLCache, sizeof (gcl_name_t), (void *) gcl_name);
	if (ep_dbg_test(Dbg, 42))
	{
		gcl_pname_t pbuf;

		gdp_gcl_printable_name(gcl_name, pbuf);
		ep_dbg_printf("gdp_gcl_cache_get: %s => %p\n", pbuf, gclh);
	}
	return gclh;
}


void
_gdp_gcl_cache_add(gcl_handle_t *gclh, gdp_iomode_t mode)
{
	// sanity checks
	EP_ASSERT_POINTER_VALID(gclh);
	EP_ASSERT_REQUIRE(!gdp_gcl_name_is_zero(gclh->gcl_name));

	// save it in the cache
	(void) ep_hash_insert(OpenGCLCache,
						sizeof (gcl_name_t), &gclh->gcl_name, gclh);
	if (ep_dbg_test(Dbg, 42))
	{
		gcl_pname_t pbuf;

		gdp_gcl_printable_name(gclh->gcl_name, pbuf);
		ep_dbg_printf("gdp_gcl_cache_add: added %s => %p\n", pbuf, gclh);
	}
}


void
_gdp_gcl_cache_drop(gcl_name_t gcl_name, gdp_iomode_t mode)
{
	gcl_handle_t *gclh;

	gclh = ep_hash_insert(OpenGCLCache, sizeof (gcl_name_t), gcl_name, NULL);
	if (ep_dbg_test(Dbg, 42))
	{
		gcl_pname_t pbuf;

		gdp_gcl_printable_name(gclh->gcl_name, pbuf);
		ep_dbg_printf("gdp_gcl_cache_drop: dropping %s => %p\n", pbuf, gclh);
	}
}



/*
**	_GDP_GCL_NEWHANDLE --- create a new gcl_handle & initialize
*/

EP_STAT
_gdp_gcl_newhandle(gcl_handle_t **pgclh)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_handle_t *gclh;

	// allocate the memory to hold the gcl_handle
	gclh = ep_mem_zalloc(sizeof *gclh);
	if (gclh == NULL)
		goto fail1;
	ep_thr_mutex_init(&gclh->mutex);
	ep_thr_cond_init(&gclh->cond);
	gclh->ts.stamp.tv_sec = TT_NOTIME;

	// success
	*pgclh = gclh;
	return estat;

fail1:
	estat = ep_stat_from_errno(errno);
	return estat;
}



/************************  PUBLIC  ************************/



/*
**	CREATE_GCL_NAME -- create a name for a new GCL
*/

void
_gdp_gcl_newname(gcl_name_t np)
{
	evutil_secure_rng_get_bytes(np, sizeof (gcl_name_t));
}


