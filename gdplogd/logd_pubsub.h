/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Headers for publish/subscribe model
*/

#ifndef _GDPD_PUBSUB_H_
#define _GDPD_PUBSUB_H_

// notify all subscribers that new data is available (or shutdown required)
extern void	sub_notify_all_subscribers(gdp_req_t *pubreq, int cmd);

// terminate a subscription
extern void	sub_end_subscription(gdp_req_t *req);

#endif // _GDPD_PUBSUB_H_
