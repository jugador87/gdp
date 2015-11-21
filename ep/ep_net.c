/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**  ----- END LICENSE BLOCK -----
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
