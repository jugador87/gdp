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
	b.i32[0] = a.i32[1];
	b.i32[1] = a.i32[0];
	return b.i64;
}
