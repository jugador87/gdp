/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2015, Eric P. Allman.  All rights reserved.
***********************************************************************/

#include "ep.h"
#include "ep_net.h"

/*
**  Network support routines.
**
**	Right now these mostly have to do with endianness.
**
**	I assume that 64 bits are transmitted in pure big-endian format;
**	for example 0x000102030405060708090A0B0C0D0E0F is transmitted as
**	00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F.  On 32-bit
**	machines we might have a NUXI problem that would transmit as
**	08 09 0A 0B 0C 0D 0E 0F 00 01 02 03 04 05 06 07.
**
**	XXX THIS NEEDS MORE RESEARCH
*/

#ifndef EP_HWCF_64_BIT_NUXI_PROBLEM
# define EP_HWCF_64_BIT_NUXI_PROBLEM	0
#endif

uint64_t
_ep_net_swap64(uint64_t v)
{
	union
	{
		uint64_t	i64;
		uint32_t	i32[2];
	} a, b;

	a.i64 = v;

#if EP_HWCF_64_BIT_NUXI_PROBLEM
	b.i32[0] = ep_net_hton32(a.i32[0]);
	b.i32[1] = ep_net_hton32(a.i32[1]);
#else
	b.i32[0] = ep_net_hton32(a.i32[1]);
	b.i32[1] = ep_net_hton32(a.i32[0]);
#endif
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
