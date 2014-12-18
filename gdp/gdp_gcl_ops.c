/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>

#include "gdp.h"
#include "gdp_gclmd.h"
#include "gdp_stat.h"
#include "gdp_priv.h"

#include <event2/event.h>

#include <errno.h>
#include <string.h>

/*
**	This implements GDP Connection Log (GCL) utilities.
*/

/************************  PRIVATE	************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.ops", "GCL operations for GDP");


/*
**	CREATE_GCL_NAME -- create a name for a new GCL
*/

void
_gdp_gcl_newname(gdp_gcl_t *gcl)
{
	evutil_secure_rng_get_bytes(gcl->gcl_name, sizeof gcl->gcl_name);
	gdp_gcl_printable_name(gcl->gcl_name, gcl->pname);
}


/*
**	_GDP_GCL_NEWHANDLE --- create a new gcl_handle & initialize
*/

EP_STAT
_gdp_gcl_newhandle(gcl_name_t gcl_name, gdp_gcl_t **pgcl)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_gcl_t *gcl;

	// allocate the memory to hold the gcl_handle
	gcl = ep_mem_zalloc(sizeof *gcl);
	if (gcl == NULL)
		goto fail1;

	ep_thr_mutex_init(&gcl->mutex, EP_THR_MUTEX_DEFAULT);
	LIST_INIT(&gcl->reqs);
	if (gcl_name != NULL)
	{
		memcpy(gcl->gcl_name, gcl_name, sizeof gcl->gcl_name);
		gdp_gcl_printable_name(gcl_name, gcl->pname);
	}
	gcl->refcnt = 1;

	// success
	*pgcl = gcl;
	ep_dbg_cprintf(Dbg, 28, "_gdp_gcl_newhandle => %p (%s)\n",
			gcl, gcl->pname);
	return estat;

fail1:
	estat = ep_stat_from_errno(errno);
	return estat;
}

/*
**  _GDP_GCL_FREEHANDLE --- drop an existing handle
*/

void
_gdp_gcl_freehandle(gdp_gcl_t *gcl)
{
	ep_dbg_cprintf(Dbg, 28, "_gdp_gcl_freehandle(%p)\n", gcl);

	// drop it from the name -> handle cache
	_gdp_gcl_cache_drop(gcl);

	// release any remaining requests
	if (!LIST_EMPTY(&gcl->reqs))
	{
		// release any remaining requests (shouldn't be any left)
		ep_dbg_cprintf(Dbg, 1, "gdp_gcl_freehandle: non-null request list\n");
		_gdp_req_freeall(&gcl->reqs);
	}

	// free any additional per-GCL resources
	if (gcl->freefunc != NULL)
		(*gcl->freefunc)(gcl);

	// release the locks and cache entry
	ep_thr_mutex_destroy(&gcl->mutex);

	// if there is any "extra" data, drop that
	//		(redundant; should be done by the freefunc)
	if (gcl->x != NULL)
	{
		ep_mem_free(gcl->x);
		gcl->x = NULL;
	}

	// finally release the memory for the handle itself
	ep_mem_free(gcl);
}


/*
**	_GDP_GCL_CREATE --- create a new GCL
*/

EP_STAT
_gdp_gcl_create(gdp_gcl_t *gcl,
				gdp_gclmd_t *gmd,
				gdp_chan_t *chan,
				uint32_t reqflags)
{
	gdp_req_t *req = NULL;
	EP_STAT estat = EP_STAT_OK;

	if (!gdp_gcl_name_is_valid(gcl->gcl_name))
		_gdp_gcl_newname(gcl);

	estat = _gdp_req_new(GDP_CMD_CREATE, gcl, chan, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// add the metadata to the output stream
	_gdp_gclmd_serialize(gmd, req->pkt->datum->dbuf);

	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail1);

	// success
	_gdp_req_free(req);
	return estat;

fail1:
	if (req != NULL)
		_gdp_req_free(req);

fail0:
	if (ep_dbg_test(Dbg, 8))
	{
		char ebuf[100];

		ep_dbg_printf("Could not create GCL: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	_GDP_GCL_OPEN --- open a GCL for reading or further appending
*/

EP_STAT
_gdp_gcl_open(gdp_gcl_t *gcl,
			int cmd,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req = NULL;

	estat = _gdp_req_new(cmd, gcl, chan, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	estat = _gdp_invoke(req);

	if (req != NULL)
		_gdp_req_free(req);

fail0:
	// log failure
	if (EP_STAT_ISOK(estat))
	{
		// success!
		ep_dbg_cprintf(Dbg, 10, "Opened GCL %s\n", gcl->pname);
	}
	else
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 10,
				"Couldn't open GCL %s: %s\n",
				gcl->pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	_GDP_GCL_CLOSE --- share operation for closing a GCL handle
*/

EP_STAT
_gdp_gcl_close(gdp_gcl_t *gcl,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req;

	EP_ASSERT_POINTER_VALID(gcl);

	estat = _gdp_req_new(GDP_CMD_CLOSE, gcl, chan, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// tell the daemon to close it
	estat = _gdp_invoke(req);

	//XXX should probably check status

	// release resources held by this handle
	_gdp_req_free(req);		// also drops gcl reference
fail0:
	return estat;
}


/*
**  _GDP_GCL_PUBLISH --- shared operation for publishing to a GCL
**
**		Used both in GDP client library and gdpd.
*/

EP_STAT
_gdp_gcl_publish(gdp_gcl_t *gcl,
			gdp_datum_t *datum,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req = NULL;

	EP_ASSERT_POINTER_VALID(gcl);
	EP_ASSERT_POINTER_VALID(datum);

	estat = _gdp_req_new(GDP_CMD_PUBLISH, gcl, chan, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	gdp_datum_free(req->pkt->datum);
	(void) ep_time_now(&datum->ts);
	req->pkt->datum = datum;
	EP_ASSERT(datum->inuse);

	estat = _gdp_invoke(req);

	req->pkt->datum = NULL;			// owned by caller
	_gdp_req_free(req);
fail0:
	return estat;
}


/*
**  _GDP_GCL_READ --- shared operation for reading a message from a GCL
**
**		Used both in GDP client library and gdpd.
**
**		Parameters:
**			gcl --- the gcl from which to read
**			datum --- the message header (to avoid dynamic memory)
**			chan --- the data channel used to contact the remote
**			reqflags --- flags for the request
*/

EP_STAT
_gdp_gcl_read(gdp_gcl_t *gcl,
			gdp_datum_t *datum,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req;

	EP_ASSERT_POINTER_VALID(gcl);
	EP_ASSERT_POINTER_VALID(datum);
	estat = _gdp_req_new(GDP_CMD_READ, gcl, chan, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	EP_TIME_INVALIDATE(&datum->ts);

	gdp_datum_free(req->pkt->datum);
	req->pkt->datum = datum;
	EP_ASSERT(datum->inuse);

	estat = _gdp_invoke(req);

	// ok, done!
	req->pkt->datum = NULL;			// owned by caller
	_gdp_req_free(req);
fail0:
	return estat;
}


/*
**	_GDP_GCL_SUBSCRIBE --- subscribe to a GCL
**
**		This also implements multiread based on the cmd parameter.
*/

EP_STAT
_gdp_gcl_subscribe(gdp_gcl_t *gcl,
		int cmd,
		gdp_recno_t start,
		int32_t numrecs,
		EP_TIME_SPEC *timeout,
		gdp_gcl_sub_cbfunc_t cbfunc,
		void *cbarg,
		gdp_chan_t *chan,
		uint32_t reqflags)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;

	EP_ASSERT_POINTER_VALID(gcl);

	estat = _gdp_req_new(cmd, gcl, chan, reqflags | GDP_REQ_PERSIST, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// add start and stop parameters to packet
	req->pkt->datum->recno = start;
	gdp_buf_put_uint32(req->pkt->datum->dbuf, numrecs);

	// issue the subscription --- no data returned
	estat = _gdp_invoke(req);
	EP_ASSERT(req->inuse);		// make sure it didn't get freed

	// now arrange for responses to appear as events or callbacks
	req->flags |= GDP_REQ_SUBSCRIPTION;
	req->cb.subs = cbfunc;
	req->udata = cbarg;

	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(req);
	}

fail0:
	return estat;
}


EP_STAT
_gdp_gcl_getmetadata(gdp_gcl_t *gcl,
		gdp_gclmd_t **gmdp,
		gdp_chan_t *chan,
		uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req;

	estat = _gdp_req_new(GDP_CMD_GETMETADATA, gcl, chan, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail1);

	*gmdp = _gdp_gclmd_deserialize(req->pkt->datum->dbuf);

fail1:
	_gdp_req_free(req);

fail0:
	return estat;
}
