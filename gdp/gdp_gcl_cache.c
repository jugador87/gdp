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

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.cache",
						"GCL cache and reference counting");



/***********************************************************************
**
**	GCL Caching
**		Let's us find the internal representation of the GCL from
**		the name.  These are not really intended for public use,
**		but they are shared with gdplogd.
**
**		FIXME This is a very stupid implementation at the moment.
**
**		FIXME Makes no distinction between io modes (we cludge this
**			  by just opening everything for r/w for now)
**
***********************************************************************/

static EP_HASH			*OpenGCLCache;		// associative cache
LIST_HEAD(gcl_use_head, gdp_gcl)			// LRU cache
						GclsByUse		= LIST_HEAD_INITIALIZER(GclByUse);
static EP_THR_MUTEX		GclCacheMutex	EP_THR_MUTEX_INITIALIZER;


/*
**  Initialize the GCL cache
*/

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
			ep_app_fatal("gdp_gcl_cache_init: could not create OpenGCLCache");
		}
	}

	// Nothing to do for LRU cache

	return estat;
}


/*
**  Add a GCL to both the associative and the LRU caches.
*/

void
_gdp_gcl_cache_add(gdp_gcl_t *gcl, gdp_iomode_t mode)
{
	// sanity checks
	EP_ASSERT_POINTER_VALID(gcl);
	EP_ASSERT_REQUIRE(gdp_name_is_valid(gcl->name));
	EP_ASSERT_REQUIRE(EP_UT_BITSET(GCLF_INUSE, gcl->flags));

	if (EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
	{
		ep_dbg_cprintf(Dbg, 8, "_gdp_gcl_cache_add(%p): already cached\n", gcl);
		return;
	}

	// save it in the associative cache
	(void) ep_hash_insert(OpenGCLCache,
						sizeof (gdp_name_t), gcl->name, gcl);

	// ... and the LRU list
	{
		struct timeval tv;

		gettimeofday(&tv, NULL);
		gcl->utime = tv.tv_sec;

		ep_thr_mutex_lock(&GclCacheMutex);
		LIST_INSERT_HEAD(&GclsByUse, gcl, ulist);
		ep_thr_mutex_unlock(&GclCacheMutex);
	}

	gcl->flags |= GCLF_INCACHE;
	ep_dbg_cprintf(Dbg, 42, "_gdp_gcl_cache_add: %s => %p\n",
			gcl->pname, gcl);
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
_gdp_gcl_cache_get(gdp_name_t gcl_name, gdp_iomode_t mode)
{
	gdp_gcl_t *gcl;

	ep_thr_mutex_lock(&GclCacheMutex);

	// see if we have a pointer to this GCL in the cache
	gcl = ep_hash_search(OpenGCLCache, sizeof (gdp_name_t), (void *) gcl_name);
	if (gcl == NULL)
		goto done;
	ep_thr_mutex_lock(&gcl->mutex);

	// sanity checking
	EP_ASSERT_INSIST(EP_UT_BITSET(GCLF_INUSE, gcl->flags));
	EP_ASSERT_INSIST(EP_UT_BITSET(GCLF_INCACHE, gcl->flags));

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
		gcl->flags |= GCLF_ISLOCKED;
		gcl->refcnt++;
		_gdp_gcl_touch(gcl);
		ep_thr_mutex_unlock(&gcl->mutex);
	}

done:
	ep_thr_mutex_unlock(&GclCacheMutex);

	if (gcl == NULL)
	{
		if (ep_dbg_test(Dbg, 42))
		{
			gdp_pname_t pname;

			gdp_printable_name(gcl_name, pname);
			ep_dbg_printf("gdp_gcl_cache_get: %s => NULL\n", pname);
		}
	}
	else
	{
		gcl->flags &= ~GCLF_ISLOCKED;
		ep_dbg_cprintf(Dbg, 42, "gdp_gcl_cache_get: %s => %p %d\n",
					gcl->pname, gcl, gcl->refcnt);
	}
	return gcl;
}


/*
** Drop a GCL from both the associative and the LRU caches
*/

void
_gdp_gcl_cache_drop(gdp_gcl_t *gcl)
{
	if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
	{
		ep_dbg_cprintf(Dbg, 8, "_gdp_gcl_cache_drop(%p): uncached\n", gcl);
		return;
	}

	// remove it from the associative cache
	(void) ep_hash_insert(OpenGCLCache, sizeof (gdp_name_t), gcl->name,
						NULL);

	// ... and the LRU list
	if (!EP_UT_BITSET(GCLF_ISLOCKED, gcl->flags))
		ep_thr_mutex_lock(&GclCacheMutex);
	LIST_REMOVE(gcl, ulist);
	if (!EP_UT_BITSET(GCLF_ISLOCKED, gcl->flags))
		ep_thr_mutex_unlock(&GclCacheMutex);

	gcl->flags &= ~GCLF_INCACHE;
	ep_dbg_cprintf(Dbg, 42, "_gdp_gcl_cache_drop: %s => %p\n",
			gcl->pname, gcl);
}


/*
**  _GDP_GCL_TOUCH --- move GCL to the front of the LRU list
*/

void
_gdp_gcl_touch(gdp_gcl_t *gcl)
{
	struct timeval tv;

	EP_ASSERT_REQUIRE(EP_UT_BITSET(GCLF_INUSE, gcl->flags));

	if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
	{
		ep_dbg_cprintf(Dbg, 8, "_gcl_gcl_touch(%p): uncached!\n", gcl);
		return;
	}

	ep_dbg_cprintf(Dbg, 46, "_gdp_gcl_touch(%p)\n", gcl);

	gettimeofday(&tv, NULL);
	gcl->utime = tv.tv_sec;

	if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
		ep_thr_mutex_lock(&GclCacheMutex);
	LIST_REMOVE(gcl, ulist);
	LIST_INSERT_HEAD(&GclsByUse, gcl, ulist);
	if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
		ep_thr_mutex_unlock(&GclCacheMutex);
}


/*
**  Reclaim cache entries older than a specified age
*/

void
_gdp_gcl_cache_reclaim(time_t maxage)
{
	struct timeval tv;
	gdp_gcl_t *g1, *g2;
	time_t mintime;

	ep_dbg_cprintf(Dbg, 68, "_gdp_gcl_cache_reclaim(maxage = %ld)\n", maxage);

	gettimeofday(&tv, NULL);
	mintime = tv.tv_sec - maxage;

	ep_thr_mutex_lock(&GclCacheMutex);
	for (g1 = LIST_FIRST(&GclsByUse); g1 != NULL; g1 = g2)
	{
		if (g1->utime > mintime)
			break;
		g2 = LIST_NEXT(g1, ulist);
		if (g1->refcnt <= 0 && !EP_UT_BITSET(GCLF_DROPPING, g1->flags))
		{
			if (ep_dbg_test(Dbg, 32))
			{
				ep_dbg_printf("_gdp_gcl_cache_reclaim: reclaiming:\n   ");
				_gdp_gcl_dump(g1, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
			}
			LIST_REMOVE(g1, ulist);
			g1->flags |= GCLF_ISLOCKED;
			_gdp_gcl_freehandle(g1);
		}
	}
	ep_thr_mutex_unlock(&GclCacheMutex);
}


/*
**  Shut down GCL cache --- immediately!
**
**		Informs subscribers of imminent shutdown.
**		Only used when the entire daemon is shutting down.
*/

void
_gdp_gcl_cache_shutdown(void (*shutdownfunc)(gdp_req_t *))
{
	gdp_gcl_t *g1, *g2;

	ep_dbg_cprintf(Dbg, 30, "\n_gdp_gcl_shutdown\n");

	// don't bother with mutexes --- we need to shut down now!

	// free all GCLs and all reqs linked to them
	for (g1 = LIST_FIRST(&GclsByUse); g1 != NULL; g1 = g2)
	{
		g2 = LIST_NEXT(g1, ulist);
		LIST_REMOVE(g1, ulist);
		_gdp_req_freeall(&g1->reqs, shutdownfunc);
		_gdp_gcl_freehandle(g1);
	}
}


/*
**  _GDP_GCL_INCREF --- increment the reference count on a GCL
*/

void
_gdp_gcl_incref(gdp_gcl_t *gcl)
{
	ep_thr_mutex_lock(&gcl->mutex);
	gcl->refcnt++;
	_gdp_gcl_touch(gcl);
	ep_dbg_cprintf(Dbg, 44, "_gdp_gcl_incref(%p): %d\n", gcl, gcl->refcnt);
	ep_thr_mutex_unlock(&gcl->mutex);
}


/*
**  _GDP_GCL_DECREF --- decrement the reference count on a GCL
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

	ep_dbg_cprintf(Dbg, 44, "_gdp_gcl_decref(%p): %d\n",
			gcl, gcl->refcnt);
	ep_thr_mutex_unlock(&gcl->mutex);
	if (gcl->refcnt == 0 && !EP_UT_BITSET(GCLF_DEFER_FREE, gcl->flags))
		_gdp_gcl_freehandle(gcl);
}


/*
**  Show contents of LRU cache (for debugging)
*/

void
_gdp_gcl_cache_dump(FILE *fp)
{
	gdp_gcl_t *gcl;

	fprintf(fp, "\n<<< Showing cached GCLs by usage >>>\n");
	LIST_FOREACH(gcl, &GclsByUse, ulist)
	{
		struct tm *tm;
		char tbuf[40];

		if ((tm = localtime(&gcl->utime)) != NULL)
			strftime(tbuf, sizeof tbuf, "%Y%m%d-%H%M%S", tm);
		else
			snprintf(tbuf, sizeof tbuf, "%"PRIu64, (int64_t) gcl->utime);
		fprintf(fp, "%s %p %s %d\n", tbuf, gcl, gcl->pname, gcl->refcnt);
	}
	fprintf(fp, "<<< End of list >>>\n");
}
