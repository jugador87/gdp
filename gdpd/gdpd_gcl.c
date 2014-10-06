/* vim: set ai sw=4 sts=4 ts=4 : */

#include "gdpd.h"
#include "gdpd_physlog.h"

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gdpd.gcl", "GDP Daemon GCL handling");

// list of current GCLs sorted by usage time
LIST_HEAD(gcl_use_head, gdp_gcl_xtra)
						GclsByUse;
EP_THR_MUTEX			GclsByUseMutex EP_THR_MUTEX_INITIALIZER;


/*
**  GCL_ALLOC --- allocate a new GCL handle in memory
*/

EP_STAT
gcl_alloc(gcl_name_t gcl_name, gdp_iomode_t iomode, gdp_gcl_t **pgclh)
{
	EP_STAT estat;
	gdp_gcl_t *gclh;
	struct timeval tv;

	// get the standard handle
	estat = _gdp_gcl_newhandle(gcl_name, &gclh);
	EP_STAT_CHECK(estat, goto fail0);
	gclh->iomode = iomode;

	// add the gdpd-specific information
	gclh->x = ep_mem_zalloc(sizeof *gclh->x);
	if (gclh->x == NULL)
	{
		estat = EP_STAT_OUT_OF_MEMORY;
		goto fail0;
	}
	gclh->x->gcl = gclh;
	gettimeofday(&tv, NULL);
	gclh->x->utime = tv.tv_sec;

	// link it into the usage chain
	ep_thr_mutex_lock(&GclsByUseMutex);
	LIST_INSERT_HEAD(&GclsByUse, gclh->x, ulist);
	ep_thr_mutex_unlock(&GclsByUseMutex);

	// make sure that if this is freed it gets removed from GclsByUse
	gclh->freefunc = gcl_close;

	// OK, return the value
	*pgclh = gclh;

fail0:
	return estat;
}


/*
**  GCL_OPEN --- open a new GCL
*/

EP_STAT
gcl_open(gcl_name_t gcl_name, gdp_iomode_t iomode, gdp_gcl_t **pgclh)
{
	EP_STAT estat;
	gdp_gcl_t *gclh;
	extern void gcl_close(gdp_gcl_t *gclh);

	estat = gcl_alloc(gcl_name, iomode, &gclh);
	EP_STAT_CHECK(estat, goto fail0);

	// so far, so good...  do the physical open
	estat = gcl_physopen(gcl_name, gclh);
	EP_STAT_CHECK(estat, goto fail1);

	// success!
	*pgclh = gclh;
	return estat;

fail1:
	_gdp_gcl_freehandle(gclh);
fail0:
	return estat;
}


/*
**  GCL_CLOSE --- close a GDP version of a GCL handle
**
**		Called from _gdp_gcl_freehandle, generally when the reference
**		count drops to zero and the GCL is reclaimed.
*/

void
gcl_close(gdp_gcl_t *gclh)
{
	// remove it from the usage list
	ep_thr_mutex_lock(&GclsByUseMutex);
	LIST_REMOVE(gclh->x, ulist);
	ep_thr_mutex_unlock(&GclsByUseMutex);

	// close the underlying files
	if (gclh->x->fp != NULL)
		gcl_physclose(gclh);

	// physical memory will be freed by _gdp_gcl_freehandle
}


/*
**  GCL_TOUCH --- update the usage time of a GCL
*/

void
gcl_touch(gdp_gcl_t *gclh)
{
	struct timeval tv;

	ep_dbg_cprintf(Dbg, 46, "gcl_touch(%p)\n", gclh);

	// mark the current time
	gettimeofday(&tv, NULL);
	gclh->x->utime = tv.tv_sec;

	// move this entry to the front of the usage list
	ep_thr_mutex_lock(&GclsByUseMutex);
	LIST_REMOVE(gclh->x, ulist);
	LIST_INSERT_HEAD(&GclsByUse, gclh->x, ulist);
	ep_thr_mutex_unlock(&GclsByUseMutex);
}


void
gcl_showusage(FILE *fp)
{
	struct gdp_gcl_xtra *x;

	fprintf(fp, "\n*** Showing cached GCLs by usage:\n");
	LIST_FOREACH(x, &GclsByUse, ulist)
	{
		struct tm *tm;
		char tbuf[40];

		if ((tm = localtime(&x->utime)) == NULL)
			snprintf(tbuf, sizeof tbuf, "%"PRIu64, (int64_t) x->utime);
		else
			strftime(tbuf, sizeof tbuf, "%Y%m%d-%H%M%S", tm);
		fprintf(fp, "%s %p %s\n", tbuf, x->gcl, x->gcl->pname);
	}
	fprintf(fp, "*** End of list\n");
}


EP_STAT
get_open_handle(gdp_req_t *req, gdp_iomode_t iomode)
{
	EP_STAT estat;

	EP_ASSERT(req->gclh == NULL);

	// see if we can find the handle in the cache
	req->gclh = _gdp_gcl_cache_get(req->pkt->gcl_name, iomode);
	if (req->gclh != NULL)
	{
		if (ep_dbg_test(Dbg, 40))
		{
			gcl_pname_t pname;

			gdp_gcl_printable_name(req->pkt->gcl_name, pname);
			ep_dbg_printf("get_open_handle: using cached GCL:\n\t%s => %p\n",
					pname, req->gclh);
		}

		// mark it as most recently used
		gcl_touch(req->gclh);
		return EP_STAT_OK;
	}

	// not in cache?  create a new one.
	if (ep_dbg_test(Dbg, 40))
	{
		gcl_pname_t pname;

		gdp_gcl_printable_name(req->pkt->gcl_name, pname);
		ep_dbg_printf("get_open_handle: opening %s\n", pname);
	}
	estat = gcl_open(req->pkt->gcl_name, iomode, &req->gclh);
	if (EP_STAT_ISOK(estat))
		_gdp_gcl_cache_add(req->gclh, iomode);
	if (ep_dbg_test(Dbg, 40))
	{
		gcl_pname_t pname;
		char ebuf[60];

		gdp_gcl_printable_name(req->pkt->gcl_name, pname);
		ep_stat_tostr(estat, ebuf, sizeof ebuf);
		ep_dbg_printf("get_open_handle: %s:\n\t@%p: %s\n",
				pname, req->gclh, ebuf);
	}
	return estat;
}
