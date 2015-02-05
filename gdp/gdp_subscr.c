/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>

#include "gdp.h"
#include "gdp_event.h"
#include "gdp_priv.h"

#include <sys/errno.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.subscr", "GDP subscriptions");

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

	errno = 0;				// avoid spurious messages

	EP_ASSERT_POINTER_VALID(gcl);

	estat = _gdp_req_new(cmd, gcl, chan, NULL, reqflags | GDP_REQ_PERSIST, &req);
	EP_STAT_CHECK(estat, goto fail0);

	// add start and stop parameters to packet
	req->pdu->datum->recno = start;
	gdp_buf_put_uint32(req->pdu->datum->dbuf, numrecs);

	// issue the subscription --- no data returned
	estat = _gdp_invoke(req);
	EP_ASSERT(EP_UT_BITSET(GDP_REQ_INUSE, req->flags));		// make sure it didn't get freed

	if (!EP_STAT_ISOK(estat))
	{
		_gdp_req_free(req);
	}
	else
	{
		// now arrange for responses to appear as events or callbacks
		req->flags |= GDP_REQ_CLT_SUBSCR;
		req->sub_cb = cbfunc;
		req->udata = cbarg;

		// if using callbacks, make sure we have a callback thread running
		if (cbfunc != NULL)
			_gdp_event_start_cb_thread();
	}

fail0:
	return estat;
}


/*
**  Subscription disappeared; remove it from list
*/

void
_gdp_subscr_lost(gdp_req_t *req)
{
	//XXX IMPLEMENT ME!              
}


/*
**  Periodically ping all open subscriptions to make sure they are
**  still happy.
*/

void
_gdp_subscr_poke(gdp_chan_t *chan)
{
	gdp_req_t *req;
	gdp_req_t *nextreq;
	EP_TIME_SPEC now;
	EP_TIME_SPEC t_poke;	// poke if older than this
	EP_TIME_SPEC t_dead;	// abort if older than this
	long delta_poke = ep_adm_getlongparam("swarm.gdp.subscr.pokeintvl", 10000L);
	long delta_dead = ep_adm_getlongparam("swarm.gdp.subscr.deadintvl", 60000L);

	ep_time_now(&now);
	ep_time_from_nsec(delta_poke * INT64_C(-1000000), &t_poke);	// msec
	ep_time_add_delta(&now, &t_poke);
	ep_time_from_nsec(delta_dead * INT64_C(-1000000), &t_dead);	// msec
	ep_time_add_delta(&now, &t_dead);

	for (req = LIST_FIRST(&chan->reqs); req != NULL; req = nextreq)
	{
		EP_STAT estat;

		nextreq = LIST_NEXT(req, chanlist);
		if (!EP_UT_BITSET(GDP_REQ_CLT_SUBSCR, req->flags))
			continue;

		if (ep_time_before(&t_poke, &req->sub_ts))
		{
			// we've seen activity recently, no need to poke
			continue;
		}

		if (ep_time_before(&req->sub_ts, &t_dead))
		{
			// this subscription is dead
			//XXX ideally this would initiate a new subscription
			_gdp_subscr_lost(req);
			continue;
		}

		// t_dead < sub_ts <= t_poke: create new poke command
		gdp_req_t *pokereq;
		estat = _gdp_req_new(GDP_CMD_POKE_SUBSCR, req->gcl, chan, NULL,
							0, &pokereq);
		if (!EP_STAT_ISOK(estat))
		{
			//XXX unclear what to do here
			continue;
		}
		estat = _gdp_req_send(pokereq);
		if (!EP_STAT_ISOK(estat))
		{
			//XXX also unclear
		}
		_gdp_req_free(pokereq);
	}
}


EP_STAT
_gdp_subscr_event(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;
	uint32_t gevflags = 0;

	// link the request onto the event queue
	gdp_event_t *gev;
	int evtype;

	// make note that we've seen activity for this subscription
	ep_time_now(&req->sub_ts);

	// for the moment we only understand data responses (for subscribe)
	switch (req->pdu->cmd)
	{
	case GDP_ACK_SUCCESS:
		// this is in response to a PING on the subscription
		return estat;

	case GDP_ACK_CONTENT:
		evtype = GDP_EVENT_DATA;
		gevflags |= GDP_EVENT_F_KEEPPDU;
		break;

	case GDP_ACK_DELETED:
		// end of subscription
		evtype = GDP_EVENT_EOS;
		break;

	case GDP_NAK_S_LOSTSUB:
		evtype = GDP_EVENT_SHUTDOWN;
		break;

	default:
		ep_dbg_cprintf(Dbg, 1,
				"_gdp_subscr_event: unexpected ack %d in subscription\n",
				req->pdu->cmd);
		estat = GDP_STAT_PROTOCOL_FAIL;
		return estat;
	}

	estat = _gdp_event_new(&gev);
	EP_STAT_CHECK(estat, return estat);

	gev->type = evtype;
	gev->gcl = req->gcl;
	gev->datum = req->pdu->datum;
	gev->udata = req->udata;
	gev->cb = req->sub_cb;
	gev->flags = gevflags;

	// schedule the event for delivery
	_gdp_event_trigger(gev);

	// the callback must call gdp_event_free(gev)

	return estat;
}
