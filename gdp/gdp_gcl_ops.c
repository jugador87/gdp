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
	gcl->freefunc = NULL;
	if (gcl->gclmd != NULL)
		gdp_gclmd_free(gcl->gclmd);
	gcl->gclmd = NULL;
	if (gcl->digest != NULL)
		ep_crypto_md_free(gcl->digest);
	gcl->digest = NULL;

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
**  _GDP_GCL_DUMP --- print a GCL (for debugging)
*/

void
_gdp_gcl_dump(
		const gdp_gcl_t *gcl,
		FILE *fp,
		int detail,
		int indent)
{
	if (detail >= GDP_PR_BASIC)
		fprintf(fp, "GCL@%p: ", gcl);
	if (gcl == NULL)
	{
		fprintf(fp, "NULL\n");
	}
	else
	{
		if (!gdp_name_is_valid(gcl->name))
		{
			fprintf(fp, "no name\n");
		}
		else
		{
			EP_ASSERT_POINTER_VALID(gcl);
			fprintf(fp, "%s\n", gcl->pname);
		}

		if (detail >= GDP_PR_BASIC)
		{
			fprintf(fp, "\tiomode = %d, refcnt = %d, reqs = %p, nrecs = %"
					PRIgdp_recno "\n",
					gcl->iomode, gcl->refcnt, LIST_FIRST(&gcl->reqs),
					gcl->nrecs);
			if (detail >= GDP_PR_DETAILED)
			{
				fprintf(fp, "\tfreefunc = %p, gclmd = %p, digest = %p, x = %p\n",
						gcl->freefunc, gcl->gclmd, gcl->digest, gcl->x);
			}
		}
	}
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

	// create a new pseudo-GCL for the daemon so we can correlate the results
	estat = _gdp_gcl_newhandle(logdname, &gcl);
	EP_STAT_CHECK(estat, goto fail0);

	// create the request
	estat = _gdp_req_new(GDP_CMD_CREATE, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// send the name of the log to be created in the payload
	gdp_buf_write(req->pdu->datum->dbuf, gclname, sizeof (gdp_name_t));

	// add the metadata to the output stream
	_gdp_gclmd_serialize(gmd, req->pdu->datum->dbuf);

	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail0);

	// success --- change the GCL name to the true name
	memcpy(gcl->name, gclname, sizeof gcl->name);

	// free resources and return results
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
			EP_CRYPTO_KEY *secretkey,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req = NULL;
	size_t pkbuflen;
	const uint8_t *pkbuf;
	int md_alg;
	int pktype;
	int pkbits;

	// send the request across to the log daemon
	errno = 0;				// avoid spurious messages
	estat = _gdp_req_new(cmd, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	estat = _gdp_invoke(req);
	EP_STAT_CHECK(estat, goto fail0);
	// success

	// save the number of records
	gcl->nrecs = req->pdu->datum->recno;

	// read in the metadata to internal format
	gcl->gclmd = _gdp_gclmd_deserialize(req->pdu->datum->dbuf);

	// if read-only, we're done
	if (cmd != GDP_CMD_OPEN_AO)
		goto finis;

	// see if we have a public key; if not we're done
	estat = gdp_gclmd_find(gcl->gclmd, GDP_GCLMD_PUBKEY,
				&pkbuflen, (const void **) &pkbuf);
	if (!EP_STAT_ISOK(estat))
	{
		ep_dbg_cprintf(Dbg, 30, "_gdp_gcl_open: no public key\n");
		goto finis;
	}

	md_alg = pkbuf[0];
	pktype = pkbuf[1];
	pkbits = (pkbuf[2] << 8) | pkbuf[3];

	if (secretkey == NULL)
	{
		secretkey = _gdp_crypto_skey_read(gcl->pname, "pem");

		if (secretkey == NULL)
		{
			// OK, now we have a problem --- we can't sign
			estat = GDP_STAT_SKEY_REQUIRED;
			ep_dbg_cprintf(Dbg, 30, "_gdp_gcl_open: no secret key\n");
			goto fail0;
		}
	}

	// validate the compatibility of the public and private keys
	{
		EP_CRYPTO_KEY *pubkey = ep_crypto_key_read_mem(pkbuf + 4, pkbuflen - 4,
				pktype, EP_CRYPTO_KEYFORM_DER, EP_CRYPTO_F_PUBLIC);

		estat = ep_crypto_key_compat(secretkey, pubkey);
		ep_crypto_key_free(pubkey);
		if (!EP_STAT_ISOK(estat))
		{
			(void) _ep_crypto_error("public & secret keys are not compatible");
			goto fail0;
		}
	}

	// set up the message digest context
	gcl->digest = ep_crypto_sign_new(secretkey, md_alg);
	if (gcl->digest == NULL)
		goto fail1;

	// add the GCL name to the hashed message digest
	ep_crypto_sign_update(gcl->digest, gcl->name, sizeof gcl->name);

	// re-serialize the metadata and include it
	struct evbuffer *evb = evbuffer_new();
	_gdp_gclmd_serialize(gcl->gclmd, evb);
	size_t evblen = evbuffer_get_length(evb);
	ep_crypto_sign_update(gcl->digest, evbuffer_pullup(evb, evblen), evblen);
	//evbuffer_drain(evb, evblen);
	evbuffer_free(evb);

	// the GCL hash structure now has the fixed part of the hash

finis:
	estat = EP_STAT_OK;

	if (false)
	{
fail1:
		estat = EP_STAT_CRYPTO_DIGEST;
	}

fail0:
	if (req != NULL)
		_gdp_req_free(req);

	// log failure
	if (EP_STAT_ISOK(estat))
	{
		// success!
		if (ep_dbg_test(Dbg, 30))
		{
			ep_dbg_printf("Opened ");
			_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
		}
		else
		{
			ep_dbg_cprintf(Dbg, 10, "Opened GCL %s\n", gcl->pname);
		}
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
	//(void) ep_time_now(&datum->ts);
	req->pdu->datum = datum;
	EP_ASSERT(datum->inuse);
	req->md = gcl->digest;
	datum->recno = gcl->nrecs + 1;

	estat = _gdp_invoke(req);
	if (EP_STAT_ISOK(estat))
		gcl->nrecs++;

	req->pdu->datum = NULL;			// owned by caller
	_gdp_req_free(req);
fail0:
	return estat;
}


/*
**  _GDP_GCL_APPEND_ASYNC --- asynchronous append
*/

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

EP_STAT
_gdp_gcl_append_async(
			gdp_gcl_t *gcl,
			gdp_datum_t *datum,
			gdp_event_cbfunc_t cbfunc,
			void *cbarg,
			gdp_chan_t *chan,
			uint32_t reqflags)
{
	EP_STAT estat;
	gdp_req_t *req = NULL;
	int i;

	errno = 0;				// avoid spurious messages

	EP_ASSERT_POINTER_VALID(gcl);
	EP_ASSERT_POINTER_VALID(datum);

	reqflags |= GDP_REQ_ASYNCIO;
	estat = _gdp_req_new(GDP_CMD_APPEND, gcl, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	gdp_datum_free(req->pdu->datum);
	(void) ep_time_now(&datum->ts);
	req->pdu->datum = datum;
	EP_ASSERT(datum->inuse);

	// arrange for responses to appear as events or callbacks
	_gdp_event_setcb(req, cbfunc, cbarg);

	estat = _gdp_req_send(req);

	// synchronous calls clear the data in the datum, so be consistent
	i = gdp_buf_drain(req->pdu->datum->dbuf, SIZE_MAX);
	if (i < 0 && ep_dbg_test(Dbg, 1))
		ep_dbg_printf("_gdp_gcl_append_async: gdp_buf_drain failure\n");

	// cleanup and return
	req->pdu->datum = NULL;			// owned by caller
	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(req);
	}
	else
	{
		req->state = GDP_REQ_IDLE;
		ep_thr_cond_signal(&req->cond);
		_gdp_req_unlock(req);
	}
fail0:
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		ep_dbg_printf("_gdp_gcl_append_async => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
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
