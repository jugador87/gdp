/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  Message management
**
**		Messages contain the header and data information for a single
**		message.
*/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <ep/ep_thr.h>

#include "gdp.h"
#include "gdp_priv.h"

#include <string.h>

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
	EP_TIME_INVALIDATE(&datum->ts);
	if (datum->dbuf == NULL)
	{
		datum->dbuf = gdp_buf_new();
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
		ep_dbg_cprintf(Dbg, 50, "  ... draining %zd bytes\n",
				evbuffer_get_length(datum->dbuf));
		evbuffer_drain(datum->dbuf, evbuffer_get_length(datum->dbuf));
	}
	ep_thr_mutex_lock(&DatumFreeListMutex);
	datum->next = DatumFreeList;
	DatumFreeList = datum;
	ep_thr_mutex_unlock(&DatumFreeListMutex);
}


gdp_recno_t
gdp_datum_getrecno(const gdp_datum_t *datum)
{
	return datum->recno;
}

void
gdp_datum_setrecno(gdp_datum_t *datum, gdp_recno_t recno)
{
	datum->recno = recno;
}

void
gdp_datum_getts(const gdp_datum_t *datum, EP_TIME_SPEC *ts)
{
	memcpy(ts, &datum->ts, sizeof *ts);
}

void
gdp_datum_setts(gdp_datum_t *datum, EP_TIME_SPEC *ts)
{
	memcpy(&datum->ts, ts, sizeof datum->ts);
}

size_t
gdp_datum_getdlen(const gdp_datum_t *datum)
{
	return gdp_buf_getlength(datum->dbuf);
}

gdp_buf_t *
gdp_datum_getbuf(const gdp_datum_t *datum)
{
	return datum->dbuf;
}

/*
**	GDP_MSG_PRINT --- print a message (for debugging)
*/

void
gdp_datum_print(const gdp_datum_t *datum,
			FILE *fp)
{
	unsigned char *d;
	int l;

	if (datum == NULL)
	{
		fprintf(fp, "null datum\n");
		return;
	}
	fprintf(fp, "GDP record %" PRIgdp_recno ", ", datum->recno);
	if (datum->dbuf == NULL)
	{
		fprintf(fp, "no data");
		d = NULL;
		l = -1;
	}
	else
	{
		l = gdp_buf_getlength(datum->dbuf);
		fprintf(fp, "len %d", l);
		d = gdp_buf_getptr(datum->dbuf, l);
	}

	if (EP_TIME_ISVALID(&datum->ts))
	{
		fprintf(fp, ", timestamp ");
		ep_time_print(&datum->ts, fp, true);
	}
	else
	{
		fprintf(fp, ", no timestamp");
	}

	if (l > 0)
	{
		fprintf(fp, "\n	 %s%.*s%s", EpChar->lquote, l, d, EpChar->rquote);
	}
	fprintf(fp, "\n");
}
