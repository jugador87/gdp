/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  Message management
**
**		Messages contain the header and data information for a single
**		message.
*/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_thr.h>

#include "gdp.h"
#include "gdp_priv.h"

static EP_DBG	Dbg = EP_DBG_INIT("gdp.datum", "GDP datum processing");

static gdp_datum_t		*DatumFreeList;
static EP_THR_MUTEX		DatumFreeListMutex EP_THR_MUTEX_INITIALIZER;

gdp_datum_t *
gdp_datum_new(void)
{
	gdp_datum_t *datum;

	// get a message off the free list, if any
	ep_thr_mutex_lock(&DatumFreeListMutex);
	if ((datum = DatumFreeList) != NULL)
	{
		DatumFreeList = datum->next;
	}
	ep_thr_mutex_unlock(&DatumFreeListMutex);

	if (datum == NULL)
	{
		// nothing on the free list; allocate anew
		datum = ep_mem_zalloc(sizeof *datum);
		ep_thr_mutex_init(&datum->mutex, EP_THR_MUTEX_DEFAULT);
	}
	datum->next = NULL;

	EP_ASSERT(!datum->inuse);

	// initialize metadata
	datum->recno = GDP_PKT_NO_RECNO;
	datum->ts.stamp.tv_sec = TT_NOTIME;
	if (datum->dbuf == NULL)
	{
		datum->dbuf = gdp_buf_new();
		gdp_buf_setlock(datum->dbuf, &datum->mutex);
	}
	ep_dbg_cprintf(Dbg, 48, "gdp_datum_new => %p\n", datum);
	datum->inuse = true;
	return datum;
}


void
gdp_datum_free(gdp_datum_t *datum)
{
	ep_dbg_cprintf(Dbg, 48, "gdp_datum_free(%p)\n", datum);
	EP_ASSERT(datum->inuse);
	datum->inuse = false;
	if (datum->dbuf != NULL)
	{
		evbuffer_drain(datum->dbuf, evbuffer_get_length(datum->dbuf));
		datum->dlen = 0;
	}
	ep_thr_mutex_lock(&DatumFreeListMutex);
	datum->next = DatumFreeList;
	DatumFreeList = datum;
	ep_thr_mutex_unlock(&DatumFreeListMutex);
}
