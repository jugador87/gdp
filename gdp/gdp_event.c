/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  GDP_EVENT.C --- event handling
*/

#include <ep/ep_thr.h>
#include <ep/ep_dbg.h>

#include "gdp.h"
#include "gdp_event.h"

static EP_DBG	Dbg = EP_DBG_INIT("gdp.event", "GDP event handling");


STAILQ_HEAD(ev_head, gdp_event);

static EP_THR_MUTEX		FreeListMutex	EP_THR_MUTEX_INITIALIZER;
static struct ev_head	FreeList		= STAILQ_HEAD_INITIALIZER(FreeList);

static EP_THR_MUTEX		ActiveListMutex	EP_THR_MUTEX_INITIALIZER;
static EP_THR_COND		ActiveListSig	EP_THR_COND_INITIALIZER;
static struct ev_head	ActiveList		= STAILQ_HEAD_INITIALIZER(ActiveList);

static EP_THR_MUTEX		CallbackListMutex	EP_THR_MUTEX_INITIALIZER;
static EP_THR_COND		CallbackListSig		EP_THR_COND_INITIALIZER;
static struct ev_head	CallbackList		= STAILQ_HEAD_INITIALIZER(CallbackList);

EP_STAT
gdp_event_new(gdp_event_t **gevp)
{
	gdp_event_t *gev = NULL;

	ep_thr_mutex_lock(&FreeListMutex);
	if ((gev = STAILQ_FIRST(&FreeList)) != NULL)
		STAILQ_REMOVE_HEAD(&FreeList, queue);
	ep_thr_mutex_unlock(&FreeListMutex);
	if (gev == NULL)
	{
		gev = ep_mem_zalloc(sizeof *gev);
	}
	*gevp = gev;
	ep_dbg_cprintf(Dbg, 48, "gdp_event_new => %p\n", gev);
	return EP_STAT_OK;
}


EP_STAT
gdp_event_free(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);

	ep_dbg_cprintf(Dbg, 48, "gdp_event_free(%p)\n", gev);

	if (gev->datum != NULL)
		gdp_datum_free(gev->datum);
	gev->datum = NULL;
	ep_thr_mutex_lock(&FreeListMutex);
	STAILQ_INSERT_HEAD(&FreeList, gev, queue);
	ep_thr_mutex_unlock(&FreeListMutex);
	return EP_STAT_OK;
}


gdp_event_t *
gdp_event_next(int64_t timeout)
{
	gdp_event_t *gev;
	EP_TIME_SPEC *absolute_timeout = NULL;
	EP_TIME_SPEC tv;

	if (timeout > 0)
	{
		ep_time_deltanow(timeout, &tv);
		absolute_timeout = &tv;
	}

	ep_thr_mutex_lock(&ActiveListMutex);
	gev = STAILQ_FIRST(&ActiveList);
	if (gev == NULL)
	{
		while ((gev = STAILQ_FIRST(&ActiveList)) == NULL)
		{
			ep_thr_cond_wait(&ActiveListSig, &ActiveListMutex, absolute_timeout);
			if (absolute_timeout != NULL)
				break;
		}
	}
	if (gev != NULL)
		STAILQ_REMOVE_HEAD(&ActiveList, queue);
	ep_thr_mutex_unlock(&ActiveListMutex);
	return gev;
}


void
gdp_event_trigger(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);

	if (req->sub_cb == NULL)
	{
		// signal the user thread
		ep_thr_mutex_lock(&ActiveListMutex);
		STAILQ_INSERT_TAIL(&ActiveList, gev, queue);
		ep_thr_cond_signal(&ActiveListSig);
		ep_thr_mutex_unlock(&ActiveListMutex);
	}
	else
	{
		// signal the callback thread
		ep_thr_mutex_lock(&CallbackListMutex);
		STAILQ_INSERT_TAIL(&CallbackList, gev, queue);
		ep_thr_cond_signal(&CallbackListSig);
		ep_thr_mutex_unlock(&CallbackListMutex);
	}
}


int
gdp_event_gettype(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->type;
}


gdp_gcl_t *
gdp_event_getgcl(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->gcl;
}


gdp_datum_t *
gdp_event_getdatum(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->datum;
}


void *
gdp_event_getudata(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	return gev->udata;
}
