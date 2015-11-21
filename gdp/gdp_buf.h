/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_BUF.H --- data buffers for the GDP
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
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
**	----- END LICENSE BLOCK -----
*/

#ifndef _GDP_BUF_H_
#define _GDP_BUF_H_

#include <event2/event.h>
#include <event2/buffer.h>
#include <ep/ep_thr.h>

typedef struct evbuffer	gdp_buf_t;

extern gdp_buf_t	*gdp_buf_new(void);

extern int			gdp_buf_reset(
						gdp_buf_t *b);

extern void			gdp_buf_free(
						gdp_buf_t *b);

extern void			gdp_buf_setlock(
						gdp_buf_t *buf,
						EP_THR_MUTEX *m);

extern size_t		gdp_buf_getlength(
						gdp_buf_t *buf);

extern size_t		gdp_buf_read(
						gdp_buf_t *buf,
						void *out,
						size_t sz);

extern size_t		gdp_buf_peek(
						gdp_buf_t *buf,
						void *out,
						size_t sz);

extern int			gdp_buf_drain(
						gdp_buf_t *buf,
						size_t sz);

extern unsigned char
					*gdp_buf_getptr(
						gdp_buf_t *buf,
						size_t sz);

extern int			gdp_buf_write(
						gdp_buf_t *buf,
						void *in,
						size_t sz);

extern int			gdp_buf_copy(
						gdp_buf_t *ibuf,
						gdp_buf_t *obuf);

extern int			gdp_buf_printf(
						gdp_buf_t *buf,
						const char *fmt, ...);

extern void			gdp_buf_dump(
						gdp_buf_t *buf,
						FILE *fp);

extern uint32_t		gdp_buf_get_uint32(
						gdp_buf_t *buf);

extern void			gdp_buf_put_uint32(
						gdp_buf_t *buf,
						const uint32_t v);

extern uint64_t		gdp_buf_get_uint48(
						gdp_buf_t *buf);

extern void			gdp_buf_put_uint48(
						gdp_buf_t *buf,
						const uint64_t v);

extern uint64_t		gdp_buf_get_uint64(
						gdp_buf_t *buf);

extern void			gdp_buf_put_uint64(
						gdp_buf_t *buf,
						const uint64_t v);

extern void			gdp_buf_get_timespec(
						gdp_buf_t *buf,
						EP_TIME_SPEC *ts);

extern void			gdp_buf_put_timespec(
						gdp_buf_t *buf,
						EP_TIME_SPEC *ts);

#endif // _GDP_BUF_H_
