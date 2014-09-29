/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>

#include "gdp.h"
#include "gdp_log.h"
#include "gdp_stat.h"
#include "gdp_priv.h"

#include <event2/event.h>

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
			gdp_log(estat, "gdp_gcl_cache_init: could not create OpenGCLCache");
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
	gdp_gcl_t *gclh;

	ep_thr_mutex_lock(&GclCacheMutex);

	// see if we have a pointer to this GCL in the cache
	gclh = ep_hash_search(OpenGCLCache, sizeof (gcl_name_t), (void *) gcl_name);
	if (gclh == NULL)
		goto done;
	ep_thr_mutex_lock(&gclh->mutex);

	// see if someone snuck in and deallocated this
	if (gclh->refcnt <= 0)
	{
		// oops, dropped from cache
		ep_thr_mutex_unlock(&gclh->mutex);
		gclh = NULL;
	}
	else
	{
		// we're good to go
		gclh->refcnt++;
		ep_thr_mutex_unlock(&gclh->mutex);
	}

done:
	ep_thr_mutex_unlock(&GclCacheMutex);

	if (ep_dbg_test(Dbg, 42))
	{
		gcl_pname_t pname;

		gdp_gcl_printable_name(gcl_name, pname);
		ep_dbg_printf("gdp_gcl_cache_get: %s => %p\n", pname, gclh);
	}
	return gclh;
}


void
_gdp_gcl_cache_add(gdp_gcl_t *gclh, gdp_iomode_t mode)
{
	// sanity checks
	EP_ASSERT_POINTER_VALID(gclh);
	EP_ASSERT_REQUIRE(!gdp_gcl_name_is_zero(gclh->gcl_name));

	// save it in the cache
	(void) ep_hash_insert(OpenGCLCache,
						sizeof (gcl_name_t), gclh->gcl_name, gclh);
	ep_dbg_cprintf(Dbg, 42, "gdp_gcl_cache_add: added %s => %p\n",
			gclh->pname, gclh);
}


void
_gdp_gcl_cache_drop(gdp_gcl_t *gclh)
{
	(void) ep_hash_insert(OpenGCLCache, sizeof (gcl_name_t), gclh->gcl_name,
						NULL);
	ep_dbg_cprintf(Dbg, 42, "gdp_gcl_cache_drop: dropping %s => %p\n",
			gclh->pname, gclh);
}


/*
**  _GDP_GCL_INCREF --- increment the reference count on a GCL
*/

void
_gdp_gcl_incref(gdp_gcl_t *gclh)
{
	ep_thr_mutex_lock(&gclh->mutex);
	gclh->refcnt++;
	ep_dbg_cprintf(Dbg, 44, "_gdp_gcl_incref: %p %d\n", gclh, gclh->refcnt);
	ep_thr_mutex_unlock(&gclh->mutex);
}


/*
**  _GDP_GCL_DECREF --- decrement the reference count on a GCL
**		XXX	Ultimately should close the GCL if the reference count
**			goes to zero.  For the time being this is a no-op.
*/

void
_gdp_gcl_decref(gdp_gcl_t *gclh)
{
	ep_thr_mutex_lock(&gclh->mutex);
	if (gclh->refcnt > 0)
	{
		gclh->refcnt--;
	}
	else
	{
		gdp_log(EP_STAT_ABORT, "_gdp_gcl_decref: %p: zero refcnt", gclh);
	}

	// XXX check for zero refcnt
	if (gclh->refcnt <= 0)
	{
		ep_dbg_cprintf(Dbg, 4, "_gdp_gcl_decref: %p: zero\n", gclh);
	}
	else
	{
		ep_dbg_cprintf(Dbg, 44, "_gdl_gcl_decref: %p: %d\n",
				gclh, gclh->refcnt);
	}
	ep_thr_mutex_unlock(&gclh->mutex);
}



/*
**	CREATE_GCL_NAME -- create a name for a new GCL
*/

void
_gdp_gcl_newname(gcl_name_t np)
{
	evutil_secure_rng_get_bytes(np, sizeof (gcl_name_t));
}

/*
**	_GDP_GCL_NEWHANDLE --- create a new gcl_handle & initialize
*/

EP_STAT
_gdp_gcl_newhandle(gcl_name_t gcl_name, gdp_gcl_t **pgclh)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_gcl_t *gclh;

	// allocate the memory to hold the gcl_handle
	gclh = ep_mem_zalloc(sizeof *gclh);
	if (gclh == NULL)
		goto fail1;

	ep_thr_mutex_init(&gclh->mutex, EP_THR_MUTEX_DEFAULT);
	LIST_INIT(&gclh->reqs);
	if (gcl_name != NULL)
	{
		memcpy(gclh->gcl_name, gcl_name, sizeof gclh->gcl_name);
		gdp_gcl_printable_name(gcl_name, gclh->pname);
	}
	gclh->refcnt = 1;

	// success
	*pgclh = gclh;
	return estat;

fail1:
	estat = ep_stat_from_errno(errno);
	return estat;
}

/*
**  _GDP_GCL_FREEHANDLE --- drop an existing handle
*/

void
_gdp_gcl_freehandle(gdp_gcl_t *gclh)
{
	// release any remaining requests
	if (!LIST_EMPTY(&gclh->reqs))
	{
		ep_dbg_cprintf(Dbg, 1, "gdp_gcl_freehandle: non-null request list\n");
	}

	// release any remaining requests (shouldn't be any left)
	_gdp_req_freeall(&gclh->reqs);

	// release the locks and cache entry
	ep_thr_mutex_destroy(&gclh->mutex);
	_gdp_gcl_cache_drop(gclh);

	// finally release the memory for the handle itself
	ep_mem_free(gclh);
}
