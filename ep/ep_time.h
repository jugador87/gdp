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

#ifndef _EP_TIME_H_
#define _EP_TIME_H_

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct
{
	int64_t		tv_sec;		// seconds since Jan 1, 1970
	int32_t		tv_nsec;	// nanoseconds
	float		tv_accuracy;	// clock accuracy in seconds
} EP_TIME_SPEC;
#pragma pack(pop)

#define EP_TIME_NOTIME		(INT64_MIN)

// return current time
extern EP_STAT	ep_time_now(EP_TIME_SPEC *tv);

// return current time plus an offset
extern EP_STAT	ep_time_deltanow(
				EP_TIME_SPEC *delta,
				EP_TIME_SPEC *tv);

// add a delta to a time (delta may be negative)
extern void	ep_time_add_delta(
				EP_TIME_SPEC *delta,
				EP_TIME_SPEC *tv);

// determine if A occurred before B
extern bool	ep_time_before(
				EP_TIME_SPEC *a,
				EP_TIME_SPEC *b);

// create a time from a scalar number of nanoseconds
extern void	ep_time_from_nsec(
				int64_t delta,
				EP_TIME_SPEC *tv);

// return putative clock accuracy
extern float	ep_time_accuracy(void);

// set the clock accuracy (may not be available)
extern void	ep_time_setaccuracy(float acc);

// format a time string into a buffer
extern char	*ep_time_format(const EP_TIME_SPEC *tv,
				char *buf,
				size_t bz,
				uint32_t flags);

// format a time string to a file
extern void	ep_time_print(const EP_TIME_SPEC *tv,
				FILE *fp,
				uint32_t);

// values for ep_time_format and ep_time_print flags
#define EP_TIME_FMT_DEFAULT	0		// pseudo-flag
#define EP_TIME_FMT_HUMAN	0x00000001	// format for humans
#define EP_TIME_FMT_NOFUZZ	0x00000002	// suppress accuracy printing

// parse a time string
extern EP_STAT	ep_time_parse(const char *timestr,
				EP_TIME_SPEC *tv);

// sleep for the indicated number of nanoseconds
extern EP_STAT	ep_time_nanosleep(int64_t);

// test to see if a timestamp is valid
#define EP_TIME_ISVALID(ts)	((ts)->tv_sec != EP_TIME_NOTIME)

// invalidate a timestamp
#define EP_TIME_INVALIDATE(ts)	((ts)->tv_sec = EP_TIME_NOTIME)

#endif //_EP_TIME_H_
