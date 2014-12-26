/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Handle publish/subscribe requests
*/

#include "logd.h"
#include "logd_pubsub.h"

#include <gdp/gdp_priv.h>
#include <ep/ep_dbg.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.pubsub",
								"GDP Log Daemon pub/sub handling");


/*
**  SUB_SEND_MESSAGE_NOTIFICATION --- inform a subscriber of a new message
*/

void
sub_send_message_notification(gdp_req_t *req, gdp_datum_t *datum)
{
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_CONTENT;
	req->pdu->datum = datum;

	if (ep_dbg_test(Dbg, 33))
	{
		ep_dbg_printf("sub_send_message_notification req:\n  ");
		_gdp_req_dump(req, ep_dbg_getfile());
	}

	estat = _gdp_pdu_out(req->pdu, req->chan);
	req->pdu->datum = NULL;				// we just borrowed the datum

	if (req->numrecs > 0 && --req->numrecs <= 0)
		sub_end_subscription(req);
}


/*
**  SUB_NOTIFY_ALL_SUBSCRIBERS --- send something to all interested parties
*/

void
sub_notify_all_subscribers(gdp_req_t *pubreq)
{
	gdp_req_t *req;

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("sub_notify_all_subscribers:\n  ");
		_gdp_req_dump(pubreq, ep_dbg_getfile());
	}

	LIST_FOREACH(req, &pubreq->gcl->reqs, gcllist)
	{
		// make sure we don't tell ourselves
		if (req == pubreq)
			continue;

		// notify subscribers
		if (EP_UT_BITSET(GDP_REQ_SUBSCRIPTION, req->flags))
			sub_send_message_notification(req, pubreq->pdu->datum);
	}
}


/*
**  SUB_END_SUBSCRIPTION --- terminate a subscription
*/

void
sub_end_subscription(gdp_req_t *req)
{
	// make it not persistent and not a subscription
	req->flags &= ~(GDP_REQ_PERSIST | GDP_REQ_SUBSCRIPTION);

	// remove the request from the work list
	ep_thr_mutex_lock(&req->gcl->mutex);
	if (req->ongcllist)
		LIST_REMOVE(req, gcllist);
	req->ongcllist = false;
	ep_thr_mutex_unlock(&req->gcl->mutex);

	// _gdp_gcl_decref(req->gcl) will happen in gdpd_req_thread cleanup

	// send an "end of subscription" event
	req->pdu->cmd = GDP_ACK_DELETED;

	if (ep_dbg_test(Dbg, 61))
	{
		ep_dbg_printf("sub_end_subscription req:\n  ");
		_gdp_req_dump(req, ep_dbg_getfile());
	}

	(void) _gdp_pdu_out(req->pdu, req->chan);
}
