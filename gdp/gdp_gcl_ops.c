/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>

#include "gdp.h"
#include "gdp_event.h"
#include "gdp_gclmd.h"
#include "gdp_priv.h"

#include <event2/event.h>

#include <string.h>
#include <sys/errno.h>

/*
**	This implements GDP Connection Log (GCL) utilities.
*/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.ops", "GCL operations for GDP");


/*
**	CREATE_GCL_NAME -- create a name for a new GCL
*/

void
_gdp_gcl_newname(gdp_gcl_t *gcl)
{
	_gdp_newname(gcl->name);
	gdp_printable_name(gcl->name, gcl->pname);
}


/*
**	_GDP_GCL_NEWHANDLE --- create a new gcl_handle & initialize
*/

EP_STAT
_gdp_gcl_newhandle(gdp_name_t gcl_name, gdp_gcl_t **pgcl)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_gcl_t *gcl;

	// allocate the memory to hold the gcl_handle
	gcl = ep_mem_zalloc(sizeof *gcl);
	if (gcl == NULL)
		goto fail1;

	ep_thr_mutex_init(&gcl->mutex, EP_THR_MUTEX_DEFAULT);
	LIST_INIT(&gcl->reqs);
	gcl->refcnt = 1;

	// create a name if we don't have one passed in
	if (gcl_name == NULL || !gdp_name_is_valid(gcl_name))
		_gdp_newname(gcl->name);
	else
		memcpy(gcl->name, gcl_name, sizeof gcl->name);
	gdp_printable_name(gcl->name, gcl->pname);

	// success
	gcl->flags |= GCLF_INUSE;
	*pgcl = gcl;
	ep_dbg_cprintf(Dbg, 28, "_gdp_gcl_newhandle => %p (%s)\n",
			gcl, gcl->pname);
	return estat;

fail1:
	estat = ep_stat_from_errno(errno);
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 4, "_gdp_gcl_newhandle failed: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}

/*
**  _GDP_GCL_FREEHANDLE --- drop an existing handle
*/

void
_gdp_gcl_freehandle(gdp_gcl_t *gcl)
{
	ep_dbg_cprintf(Dbg, 28, "_gdp_gcl_freehandle(%p)\n", gcl);

	EP_ASSERT(EP_UT_BITSET(GCLF_INUSE, gcl->flags));

	// drop it from the name -> handle cache
	_gdp_gcl_cache_drop(gcl);

	// release any remaining requests
	if (!LIST_EMPTY(&gcl->reqs))
	{
		// release any remaining requests (shouldn't be any left)
		ep_dbg_cprintf(Dbg, 1, "gdp_gcl_freehandle: non-null request list\n");
		_gdp_req_freeall(&gcl->reqs, NULL);
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
	gcl->flags &= ~GCLF_INUSE;
	ep_mem_free(gcl);
}


/*
**	_GDP_GCL_CREATE --- create a new GCL
**
**		Creation is a bit tricky, since we don't start with an existing
**		GCL, and we address the message to the desired daemon instead
**		of to the GCL itself.  Some magic needs to occur.
*/

EP_STAT
_gdp_gcl_create(gdp_name_t gclname,
				gdp_name_t logdname,
				gdp_gclmd_t *gmd,
				gdp_chan_t *chan,
				uint32_t reqflags,
				gdp_gcl_t **pgcl)
{
	gdp_req_t *req = NULL;
	gdp_gcl_t *gcl = NULL;
	EP_STAT estat = EP_STAT_OK;

	errno = 0;				// avoid spurious messages

	{
		gdp_pname_t gxname, dxname;

		ep_dbg_cprintf(Dbg, 17,
				"_gdp_gcl_create: gcl=%s\n\tlogd=%s\n",
				gclname == NULL ? "none" : gdp_printable_name(gclname, gxname),
				gdp_printable_name(logdname, dxname));
	}

	// create a new GCL so we can correlate the results
	estat = _gdp_gcl_newhandle(gclname, &gcl);
	EP_STAT_CHECK(estat, goto fail0);

	// create the request
	estat = _gdp_req_new(GDP_CMD_CREATE, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// set the target address to be the log daemon
	memcpy(req->pdu->dst, logdname, sizeof req->pdu->dst);

	// send the name of the log to be created in the payload
	gdp_buf_write(req->pdu->datum->dbuf, gcl->name, sizeof (gdp_name_t));

	// add the metadata to the output stream
	_gdp_gclmd_serialize(gmd, req->pdu->datum->dbuf);

	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail0);

	// success
	_gdp_req_free(req);
	*pgcl = gcl;
	return estat;

fail0:
	if (gcl != NULL)
		_gdp_gcl_freehandle(gcl);
	if (req != NULL)
	{
		req->gcl = NULL;
		_gdp_req_free(req);
	}

	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 8, "Could not create GCL: %s\n",
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

	errno = 0;				// avoid spurious messages

	estat = _gdp_req_new(cmd, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	estat = _gdp_invoke(req);

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

	errno = 0;				// avoid spurious messages

	EP_ASSERT_POINTER_VALID(gcl);

	estat = _gdp_req_new(GDP_CMD_CLOSE, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// tell the daemon to close it
	estat = _gdp_invoke(req);

	//XXX should probably check status (and do what with it?)

	// release resources held by this handle
	_gdp_req_free(req);		// also drops gcl reference
fail0:
	return estat;
}


/*
**  _GDP_GCL_APPEND --- shared operation for appending to a GCL
**
**		Used both in GDP client library and gdpd.
*/

EP_STAT
_gdp_gcl_append(gdp_gcl_t *gcl,
			gdp_datum_t *datum,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req = NULL;

	errno = 0;				// avoid spurious messages

	EP_ASSERT_POINTER_VALID(gcl);
	EP_ASSERT_POINTER_VALID(datum);

	estat = _gdp_req_new(GDP_CMD_APPEND, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	gdp_datum_free(req->pdu->datum);
	(void) ep_time_now(&datum->ts);
	req->pdu->datum = datum;
	EP_ASSERT(datum->inuse);

	estat = _gdp_invoke(req);

	req->pdu->datum = NULL;			// owned by caller
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

	errno = 0;				// avoid spurious messages

	EP_ASSERT_POINTER_VALID(gcl);
	EP_ASSERT_POINTER_VALID(datum);
	estat = _gdp_req_new(GDP_CMD_READ, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	EP_TIME_INVALIDATE(&datum->ts);

	gdp_datum_free(req->pdu->datum);
	req->pdu->datum = datum;
	EP_ASSERT(datum->inuse);

	estat = _gdp_invoke(req);

	// ok, done!
	req->pdu->datum = NULL;			// owned by caller
	_gdp_req_free(req);
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

	errno = 0;				// avoid spurious messages

	estat = _gdp_req_new(GDP_CMD_GETMETADATA, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail1);

	*gmdp = _gdp_gclmd_deserialize(req->pdu->datum->dbuf);

fail1:
	_gdp_req_free(req);

fail0:
	return estat;
}
