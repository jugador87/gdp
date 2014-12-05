/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>
#include <ep/ep_log.h>

#include "gdp.h"
#include "gdp_stat.h"
#include "gdp_priv.h"

#include <event2/event.h>

#include <errno.h>
#include <string.h>

/*
**	This implements GDP Connection Log Cache
*/

/************************  PRIVATE	************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.cache",
						"GCL cache and reference counting");



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
static EP_THR_MUTEX	GclCacheMutex	EP_THR_MUTEX_INITIALIZER;

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
			ep_log(estat, "gdp_gcl_cache_init: could not create OpenGCLCache");
			ep_app_abort("gdp_gcl_cache_init: could not create OpenGCLCache");
		}
	}
	return estat;
}


/*
**  _GDP_GCL_CACHE_GET --- get a GCL from the cache, if it exists
**
**		It's annoying, but you have to lock the entire cache when
**		searching to make sure someone doesn't (for example) sneak
**		in and grab a GCL that you are about to lock.  The basic
**		procedure is (1) lock cache, (2) get GCL, (3) lock GCL,
**		(4) bump refcnt, (5) unlock GCL, (6) unlock cache.
*/

gdp_gcl_t *
_gdp_gcl_cache_get(gcl_name_t gcl_name, gdp_iomode_t mode)
{
	gdp_gcl_t *gcl;

	ep_thr_mutex_lock(&GclCacheMutex);

	// see if we have a pointer to this GCL in the cache
	gcl = ep_hash_search(OpenGCLCache, sizeof (gcl_name_t), (void *) gcl_name);
	if (gcl == NULL)
		goto done;
	ep_thr_mutex_lock(&gcl->mutex);

	// see if someone snuck in and deallocated this
	if (EP_UT_BITSET(GCLF_DROPPING, gcl->flags))
	{
		// oops, dropped from cache
		ep_thr_mutex_unlock(&gcl->mutex);
		gcl = NULL;
	}
	else
	{
		// we're good to go
		gcl->refcnt++;
		ep_thr_mutex_unlock(&gcl->mutex);
	}

done:
	ep_thr_mutex_unlock(&GclCacheMutex);

	if (gcl == NULL)
	{
		if (ep_dbg_test(Dbg, 42))
		{
			gcl_pname_t pname;

			gdp_gcl_printable_name(gcl_name, pname);
			ep_dbg_printf("gdp_gcl_cache_get: %s => NULL\n", pname);
		}
	}
	else
	{
		ep_dbg_cprintf(Dbg, 42, "gdp_gcl_cache_get: %s => %p %d\n",
					gcl->pname, gcl, gcl->refcnt);
	}
	return gcl;
}


void
_gdp_gcl_cache_add(gdp_gcl_t *gcl, gdp_iomode_t mode)
{
	// sanity checks
	EP_ASSERT_POINTER_VALID(gcl);
	EP_ASSERT_REQUIRE(!gdp_gcl_name_is_zero(gcl->gcl_name));

	// save it in the cache
	(void) ep_hash_insert(OpenGCLCache,
						sizeof (gcl_name_t), gcl->gcl_name, gcl);
	gcl->flags |= GCLF_INCACHE;
	ep_dbg_cprintf(Dbg, 42, "gdp_gcl_cache_add: added %s => %p\n",
			gcl->pname, gcl);
}


void
_gdp_gcl_cache_drop(gdp_gcl_t *gcl)
{
	(void) ep_hash_insert(OpenGCLCache, sizeof (gcl_name_t), gcl->gcl_name,
						NULL);
	gcl->flags &= ~GCLF_INCACHE;
	ep_dbg_cprintf(Dbg, 42, "gdp_gcl_cache_drop: dropping %s => %p\n",
			gcl->pname, gcl);
}


/*
**  _GDP_GCL_INCREF --- increment the reference count on a GCL
*/

void
_gdp_gcl_incref(gdp_gcl_t *gcl)
{
	ep_thr_mutex_lock(&gcl->mutex);
	gcl->refcnt++;
	ep_dbg_cprintf(Dbg, 44, "_gdp_gcl_incref: %p %d\n", gcl, gcl->refcnt);
	ep_thr_mutex_unlock(&gcl->mutex);
}


/*
**  _GDP_GCL_DECREF --- decrement the reference count on a GCL
**		XXX	Ultimately should close the GCL if the reference count
**			goes to zero.  For the time being this is a no-op.
*/

void
_gdp_gcl_decref(gdp_gcl_t *gcl)
{
	ep_dbg_cprintf(Dbg, 70, "_gdp_gcl_decref(%p)...\n", gcl);
	ep_thr_mutex_lock(&gcl->mutex);
	if (gcl->refcnt > 0)
	{
		gcl->refcnt--;
	}
	else
	{
		ep_log(EP_STAT_ABORT, "_gdp_gcl_decref: %p: zero refcnt", gcl);
	}

	// XXX check for zero refcnt
	if (gcl->refcnt <= 0)
	{
		// XXX temporary for debugging.  Actual reclaim should be done
		// XXX asynchronously after a timeout.
		if (ep_dbg_test(Dbg, 101))
			gcl->flags |= GCLF_DROPPING;
		ep_dbg_cprintf(Dbg, 41, "_gdp_gcl_decref: %p: zero\n", gcl);
	}
	else
	{
		ep_dbg_cprintf(Dbg, 44, "_gdl_gcl_decref: %p: %d\n",
				gcl, gcl->refcnt);
	}
	ep_thr_mutex_unlock(&gcl->mutex);

	// XXX eventually done elsewhere
	if (EP_UT_BITSET(GCLF_DROPPING, gcl->flags))
	{
		// deallocate physical memory and remove from cache
		ep_dbg_cprintf(Dbg, 20, "_gdp_gcl_decref: deallocating\n");
		_gdp_gcl_freehandle(gcl);
	}
}
