/* vim: set ai sw=4 sts=4 ts=4 :*/

/***********************************************************************
**
**  GDP Request handling
*/

#include <string.h>

#include <gdp/gdp.h>
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
**		gclh --- the associated GCL handle
**		reqp --- a pointer to the output area
**
**	Returns:
**		status
**		The request has been allocated an id (possibly unique to gclh),
**			but the request has not been linked onto the GCL's request list.
**			This allows the caller to adjust the request without locking it.
*/

EP_STAT
_gdp_req_new(int cmd,
		gcl_handle_t *gclh,
		uint32_t flags,
		gdp_req_t **reqp)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;

	// get memory, off free list if possible
	ep_thr_mutex_lock(&ReqFreeListMutex);
	if ((req = LIST_FIRST(&ReqFreeList)) != NULL)
	{
		LIST_REMOVE(req, list);
	}
	ep_thr_mutex_unlock(&ReqFreeListMutex);
	if (req == NULL)
	{
		req = ep_mem_zalloc(sizeof *req);
		ep_thr_mutex_init(&req->mutex, EP_THR_MUTEX_DEFAULT);
		ep_thr_cond_init(&req->cond);
	}

	EP_ASSERT(req->pkt.msg == NULL);
	req->gclh = gclh;
	req->stat = EP_STAT_OK;
	req->flags = 0;
	req->pkt.cmd = cmd;
	if (gclh != NULL)
		memcpy(req->pkt.gcl_name, gclh->gcl_name, sizeof req->pkt.gcl_name);
	if (gclh == NULL || !EP_UT_BITSET(GDP_REQ_PERSIST, flags))
	{
		// just use constant zero; any value would be fine
		req->pkt.rid = GDP_PKT_NO_RID;
	}
	else
	{
		// allocate a new unique request id
		req->pkt.rid = _gdp_rid_new(gclh);
	}

	// success
	*reqp = req;
	ep_dbg_cprintf(Dbg, 48, "gdp_req_new(gcl=%p): %p\n", gclh, req);
	return estat;
}


void
_gdp_req_free(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 48, "gdp_req_free: %p: gclh=%p\n", req, req->gclh);

	ep_thr_mutex_lock(&req->mutex);

	// remove it from the old list
	if (req->gclh != NULL)
	{
		ep_thr_mutex_lock(&req->gclh->mutex);
		LIST_REMOVE(req, list);
		ep_thr_mutex_unlock(&req->gclh->mutex);
	}

	// free the associated message, assuming we own it
	if (EP_UT_BITSET(GDP_REQ_OWN_MSG, req->flags))
	{
		gdp_msg_free(req->pkt.msg);
		req->flags &= ~GDP_REQ_OWN_MSG;
	}
	req->pkt.msg = NULL;

	// add the empty request to the free list
	ep_thr_mutex_lock(&ReqFreeListMutex);
	LIST_INSERT_HEAD(&ReqFreeList, req, list);
	ep_thr_mutex_unlock(&ReqFreeListMutex);

	ep_thr_mutex_unlock(&req->mutex);
}

void
_gdp_req_freeall(struct req_head *reqlist)
{
	gdp_req_t *r1 = LIST_FIRST(reqlist);

	while (r1 != NULL)
	{
		gdp_req_t *r2 = LIST_NEXT(r1, list);
		_gdp_req_free(r1);
		r1 = r2;
	}
}


gdp_req_t *
_gdp_req_find(gcl_handle_t *gclh, gdp_rid_t rid)
{
	gdp_req_t *req;

	ep_thr_mutex_lock(&gclh->mutex);
	LIST_FOREACH(req, &gclh->reqs, list)
	{
		if (req->pkt.rid == rid)
			break;
	}
	if (req != NULL && !EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
		LIST_REMOVE(req, list);
	ep_thr_mutex_unlock(&gclh->mutex);
	ep_dbg_cprintf(Dbg, 48, "gdp_req_find(gclh=%p, rid=%" PRIgdp_rid ") => %p\n",
			gclh, rid, req);
	return req;
}


static EP_PRFLAGS_DESC	ReqFlags[] =
{
	{ GDP_REQ_DONE,			GDP_REQ_DONE,			"DONE"			},
	{ GDP_REQ_PERSIST,		GDP_REQ_PERSIST,		"PERSIST"		},
	{ GDP_REQ_OWN_MSG,		GDP_REQ_OWN_MSG,		"OWN_MSG"		},
	{ GDP_REQ_SUBSCRIPTION,	GDP_REQ_SUBSCRIPTION,	"SUBSCRIPTION"	},
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
	gdp_gcl_print(req->gclh, fp, 0, 0);
	fprintf(fp, "    ");
	_gdp_pkt_dump(&req->pkt, fp);
	fprintf(fp, "    flags=");
	ep_prflags(req->flags, ReqFlags, fp);
	fprintf(fp, "\n    udata=%p, stat=%s\n",
			req->udata,
			ep_stat_tostr(req->stat, ebuf, sizeof ebuf));
}


/***********************************************************************
**
**	Request ID handling
**
**		Very simplistic for now.  RIDs really only need to be unique
**		within a given GCL.
*/

static gdp_rid_t	MaxRid = 0;

gdp_rid_t
_gdp_rid_new(gcl_handle_t *gclh)
{
	return ++MaxRid;
}

char *
gdp_rid_tostr(gdp_rid_t rid, char *buf, size_t len)
{
	snprintf(buf, len, "%d", rid);
	return buf;
}