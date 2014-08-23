/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  GDP_EVENT.C --- event handling
*/

#include <ep/ep_thr.h>
#include <ep/ep_dbg.h>
#include "gdp.h"
#include "gdp_event.h"

static EP_DBG	Dbg = EP_DBG_INIT("gdp.event", "GDP event handling");


static EP_THR_MUTEX		FreeListMutex	EP_THR_MUTEX_INITIALIZER;
static gdp_event_t		*FreeList;

static EP_THR_MUTEX		ActiveListMutex	EP_THR_MUTEX_INITIALIZER;
static EP_THR_COND		ActiveListSig	EP_THR_COND_INITIALIZER;
static gdp_event_t		*ActiveList;
static gdp_event_t		*ActiveTail;

EP_STAT
gdp_event_new(gdp_event_t **gevp)
{
	gdp_event_t *gev = NULL;

	ep_thr_mutex_lock(&FreeListMutex);
	if (FreeList != NULL)
	{
		gev = FreeList;
		if (gev != NULL)
		{
			FreeList = gev->next;
			gev->next = NULL;
		}
	}
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
	EP_ASSERT(gev->next == NULL);

	ep_dbg_cprintf(Dbg, 48, "gdp_event_free(%p)\n", gev);

	if (gev->datum != NULL)
		gdp_datum_free(gev->datum);
	gev->datum = NULL;
	ep_thr_mutex_lock(&FreeListMutex);
	gev->next = FreeList;
	FreeList = gev;
	ep_thr_mutex_unlock(&FreeListMutex);
	return EP_STAT_OK;
}


gdp_event_t *
gdp_event_next(bool wait)
{
	gdp_event_t *gev;

	ep_thr_mutex_lock(&ActiveListMutex);
	gev = ActiveList;
	if (gev == NULL && wait)
	{
		while (ActiveList == NULL)
			ep_thr_cond_wait(&ActiveListSig, &ActiveListMutex);
		gev = ActiveList;
	}
	if (gev != NULL)
	{
		ActiveList = gev->next;
		if (ActiveList == NULL)
			ActiveTail = NULL;
	}
	ep_thr_mutex_unlock(&ActiveListMutex);
	return gev;
}


void
gdp_event_add(gdp_event_t *gev)
{
	EP_ASSERT_POINTER_VALID(gev);
	EP_ASSERT(gev->next == NULL);

	ep_thr_mutex_lock(&ActiveListMutex);
	if (ActiveTail != NULL)
	{
		ActiveTail->next = gev;
		ActiveTail = gev;
	}
	else
	{
		EP_ASSERT(ActiveList == NULL);
		gev->next = NULL;
		ActiveList = ActiveTail = gev;
	}
	ep_thr_cond_signal(&ActiveListSig);
	ep_thr_mutex_unlock(&ActiveListMutex);
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
