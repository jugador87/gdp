/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2015, Eric P. Allman.  All rights reserved.
***********************************************************************/

#ifndef _EP_NET_H_
#define _EP_NET_H_

#include <arpa/inet.h>

#include <ep/ep_time.h>

#define ep_net_hton16(v)	htons(v)
#define ep_net_ntoh16(v)	ntohs(v)
#define ep_net_hton32(v)	htonl(v)
#define ep_net_ntoh32(v)	ntohl(v)
#define ep_net_hton64(v)	((htonl(1) == 1) ? v : _ep_net_swap64(v))
#define ep_net_ntoh64(v)	((htonl(1) == 1) ? v : _ep_net_swap64(v))

#define ep_net_hton_timespec(v)	((htonl(1) == 1) ? 0 : _ep_net_swap_timespec(v))
#define ep_net_ntoh_timespec(v)	((htonl(1) == 1) ? 0 : _ep_net_swap_timespec(v))


extern uint64_t		_ep_net_swap64(uint64_t v);
extern int		_ep_net_swap_timespec(EP_TIME_SPEC *v);

#endif //_EP_NET_H_
