/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Handle publish/subscribe requests
*/

#include "gdpd.h"
#include "gdpd_pubsub.h"

#include <gdp/gdp_priv.h>
#include <ep/ep_dbg.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdpd.pubsub", "GDP Daemon pub/sub handling");


/*
**  SUB_SEND_MESSAGE_NOTIFICATION --- inform a subscriber of a new message
*/

void
sub_send_message_notification(gdp_req_t *req, gdp_datum_t *datum)
{
	EP_STAT estat;

	if (ep_dbg_test(Dbg, 63))
	{
		ep_dbg_printf("sub_send_message_notification req:\n  ");
		_gdp_req_dump(req, ep_dbg_getfile());
	}

	req->pkt->cmd = GDP_ACK_DATA_CONTENT;
	req->pkt->datum = datum;

	estat = _gdp_pkt_out(req->pkt, bufferevent_get_output(req->chan));

	//TODO: IMPLEMENT ME!
}


/*
**  SUB_NOTIFY_ALL_SUBSCRIBERS --- send something to all interested parties
*/

void
sub_notify_all_subscribers(gdp_req_t *pubreq)
{
	gdp_req_t *req;

	if (ep_dbg_test(Dbg, 62))
	{
		ep_dbg_printf("sub_notify_all_subscribers:\n  ");
		_gdp_req_dump(pubreq, ep_dbg_getfile());
	}

	LIST_FOREACH(req, &pubreq->gclh->reqs, list)
	{
		// make sure we don't tell ourselves
		if (req == pubreq)
			continue;

		// notify subscribers
		if (EP_UT_BITSET(GDP_REQ_SUBSCRIPTION, req->flags))
			sub_send_message_notification(req, pubreq->pkt->datum);
	}
}
