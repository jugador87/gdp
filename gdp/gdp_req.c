/* vim: set ai sw=4 sts=4 ts=4 :*/

/***********************************************************************
**
**  GDP Request handling
*/

#include <string.h>

#include "gdp.h"
#include "gdp_priv.h"

#include <ep/ep_dbg.h>
#include <ep/ep_prflags.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.req", "GDP request processing");

// unused request structures
static struct req_head	ReqFreeList = LIST_HEAD_INITIALIZER(ReqFreeList);
static EP_THR_MUTEX		ReqFreeListMutex	EP_THR_MUTEX_INITIALIZER;

/*
**  _GDP_REQ_NEW --- allocate a new request
**
**	Parameters:
**		cmd --- the command to be issued
**		gcl --- the associated GCL handle, if any
**		chan --- the channel associated with the request
**		pdu --- the existing PDU; if none, one will be allocated
**		flags --- modifier flags
**		reqp --- a pointer to the output area
**
**	Returns:
**		status
**		The request has been allocated an id (possibly unique to gcl),
**			but the request has not been linked onto the GCL's request list.
**			This allows the caller to adjust the request without locking it.
*/

EP_STAT
_gdp_req_new(int cmd,
		gdp_gcl_t *gcl,
		gdp_chan_t *chan,
		gdp_pdu_t *pdu,
		uint32_t flags,
		gdp_req_t **reqp)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;
	bool newpdu = pdu == NULL;

	// get memory, off free list if possible
	ep_thr_mutex_lock(&ReqFreeListMutex);
	if ((req = LIST_FIRST(&ReqFreeList)) != NULL)
	{
		LIST_REMOVE(req, gcllist);
	}
	ep_thr_mutex_unlock(&ReqFreeListMutex);
	if (req == NULL)
	{
		req = ep_mem_zalloc(sizeof *req);
		ep_thr_mutex_init(&req->mutex, EP_THR_MUTEX_DEFAULT);
		ep_thr_cond_init(&req->cond);
	}

	EP_ASSERT(!req->inuse);
	EP_ASSERT(!req->ongcllist);
	EP_ASSERT(!req->onchanlist);
	if (pdu != NULL)
		req->pdu = pdu;
	else
		req->pdu = pdu = _gdp_pdu_new();
	req->gcl = gcl;
	req->stat = EP_STAT_OK;
	req->flags = flags;
	req->chan = chan;
	if (chan != NULL)
	{
		LIST_INSERT_HEAD(&chan->reqs, req, chanlist);
		req->onchanlist = true;
	}
	if (newpdu)
	{
		req->pdu->cmd = cmd;
		if (gcl != NULL)
			memcpy(req->pdu->dst, gcl->name, sizeof req->pdu->dst);
		if ((gcl == NULL || !EP_UT_BITSET(GDP_REQ_PERSIST, flags)) &&
				!EP_UT_BITSET(GDP_REQ_ALLOC_RID, flags))
		{
			// just use constant zero; any value would be fine
			req->pdu->rid = GDP_PDU_NO_RID;
		}
		else
		{
			// allocate a new unique request id
			req->pdu->rid = _gdp_rid_new(gcl, chan);
		}
	}

	// success
	req->inuse = true;
	*reqp = req;
	ep_dbg_cprintf(Dbg, 48, "gdp_req_new(gcl=%p) => %p\n", gcl, req);
	return estat;
}


void
_gdp_req_free(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 48, "gdp_req_free(%p)  gcl=%p\n", req, req->gcl);

	ep_thr_mutex_lock(&req->mutex);
	EP_ASSERT(req->inuse);

	// remove the request from the channel subscription list
	if (req->onchanlist)
		LIST_REMOVE(req, chanlist);
	req->onchanlist = false;

	// remove the request from the GCL list
	if (req->ongcllist)
		LIST_REMOVE(req, gcllist);
	req->ongcllist = false;

	// free the associated packet
	if (req->pdu != NULL)
		_gdp_pdu_free(req->pdu);
	req->pdu = NULL;

	// dereference the gcl
	if (req->gcl != NULL)
	{
		_gdp_gcl_decref(req->gcl);
		req->gcl = NULL;
	}

	req->inuse = false;
	ep_thr_mutex_unlock(&req->mutex);

	// add the empty request to the free list
	ep_thr_mutex_lock(&ReqFreeListMutex);
	LIST_INSERT_HEAD(&ReqFreeList, req, gcllist);
	ep_thr_mutex_unlock(&ReqFreeListMutex);
}

void
_gdp_req_freeall(struct req_head *reqlist)
{
	gdp_req_t *r1 = LIST_FIRST(reqlist);

	ep_dbg_cprintf(Dbg, 49, ">>> _gdp_req_freeall(%p)\n", reqlist);
	while (r1 != NULL)
	{
		gdp_req_t *r2 = LIST_NEXT(r1, gcllist);
		_gdp_req_free(r1);
		r1 = r2;
	}
	ep_dbg_cprintf(Dbg, 49, "<<< _gdp_req_freeall(%p)\n", reqlist);
}


/*
**   _GDP_REQ_SEND --- send a request to the GDP daemon
**
**		This makes no attempt to read results.
*/

EP_STAT
_gdp_req_send(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_gcl_t *gcl = req->gcl;

	if (ep_dbg_test(Dbg, 45))
	{
		ep_dbg_printf("_gdp_req_send: gcl=%p, ", gcl);
		_gdp_req_dump(req, ep_dbg_getfile());
	}

	if (gcl != NULL && !req->ongcllist)
	{
		// link the request to the GCL
		ep_thr_mutex_lock(&gcl->mutex);
		LIST_INSERT_HEAD(&gcl->reqs, req, gcllist);
		req->ongcllist = true;
		ep_thr_mutex_unlock(&gcl->mutex);

		// register this handle so we can process the results
		//		(it's likely that it's already in the cache)
		_gdp_gcl_cache_add(gcl, 0);
	}

	// write the message out
	estat = _gdp_pdu_out(req->pdu, req->chan);

	// done
	return estat;
}


/*
**  _GDP_REQ_FIND --- find a request in a GCL
*/

gdp_req_t *
_gdp_req_find(gdp_gcl_t *gcl, gdp_rid_t rid)
{
	gdp_req_t *req;

	ep_thr_mutex_lock(&gcl->mutex);
	LIST_FOREACH(req, &gcl->reqs, gcllist)
	{
		if (req->pdu->rid == rid)
			break;
	}
	if (req != NULL && !EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
	{
		EP_ASSERT(req->ongcllist);
		LIST_REMOVE(req, gcllist);
		req->ongcllist = false;
	}
	ep_thr_mutex_unlock(&gcl->mutex);
	ep_dbg_cprintf(Dbg, 48, "gdp_req_find(gcl=%p, rid=%" PRIgdp_rid ") => %p\n",
			gcl, rid, req);
	return req;
}


static EP_PRFLAGS_DESC	ReqFlags[] =
{
	{ GDP_REQ_DONE,			GDP_REQ_DONE,			"DONE"			},
	{ GDP_REQ_PERSIST,		GDP_REQ_PERSIST,		"PERSIST"		},
	{ GDP_REQ_SUBSCRIPTION,	GDP_REQ_SUBSCRIPTION,	"SUBSCRIPTION"	},
	{ GDP_REQ_SUBUPGRADE,	GDP_REQ_SUBUPGRADE,		"SUBUPGRADE"	},
	{ GDP_REQ_ALLOC_RID,	GDP_REQ_ALLOC_RID,		"ALLOC_RID"		},
	{ 0,					0,						NULL			}
};

void
_gdp_req_dump(gdp_req_t *req, FILE *fp)
{
	char ebuf[200];

	fprintf(fp, "req@%p: ", req);
	if (req == NULL)
	{
		fprintf(fp, "null\n");
		return;
	}
	fprintf(fp, "\n    ");
	gdp_gcl_print(req->gcl, fp, 1, 0);
	fprintf(fp, "    ");
	_gdp_pdu_dump(req->pdu, fp);
	fprintf(fp, "    flags=");
	ep_prflags(req->flags, ReqFlags, fp);
	fprintf(fp, "\n    %sinuse, %spostproc, %songcllist, %sonchanlist\n",
			req->inuse ? "" : "!",
			req->postproc ? "" : "!",
			req->ongcllist ? "" : "!",
			req->onchanlist ? "" : "!");
	fprintf(fp, "    chan=%p, cb=%p, udata=%p\n    stat=%s\n",
			req->chan, req->cb.generic, req->udata,
			ep_stat_tostr(req->stat, ebuf, sizeof ebuf));
}


/***********************************************************************
**
**	Request ID handling
**
**		Very simplistic for now.  RIDs really only need to be unique
**		within a given GCL/channel tuple.
*/

static gdp_rid_t	MaxRid = 0;

gdp_rid_t
_gdp_rid_new(gdp_gcl_t *gcl, gdp_chan_t *chan)
{
	return ++MaxRid;
}

char *
gdp_rid_tostr(gdp_rid_t rid, char *buf, size_t len)
{
	snprintf(buf, len, "%d", rid);
	return buf;
}
