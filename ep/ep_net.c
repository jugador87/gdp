/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2015, Eric P. Allman.  All rights reserved.
***********************************************************************/

#include "ep.h"
#include "ep_net.h"

uint64_t
_ep_net_swap64(uint64_t v)
{
	union
	{
		uint64_t	i64;
		uint32_t	i32[2];
	} a, b;

	a.i64 = v;
	b.i32[0] = ep_net_hton32(a.i32[1]);
	b.i32[1] = ep_net_hton32(a.i32[0]);
	return b.i64;
}


int
_ep_net_swap_timespec(EP_TIME_SPEC *ts)
{
	uint32_t *p32;

	ts->tv_sec = ep_net_hton64(ts->tv_sec);
	ts->tv_nsec = ep_net_hton32(ts->tv_nsec);
	p32 = (uint32_t *) &ts->tv_accuracy;
	*p32 = ep_net_hton32(*p32);
	return 0;
}
