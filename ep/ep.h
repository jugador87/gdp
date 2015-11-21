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

/*
**  EP.H -- general definitions for EP library.
**
**  	A lot of this is just to override some of the full library names.
**  	Some should probably be broken out into other headers.
*/

#ifndef _EP_H_
#define _EP_H_

#if __linux__
# define _BSD_SOURCE		1	// needed to compile on Linux
# define _POSIX_C_SOURCE	200809L	// specify a modern environment
#endif

#include <ep/ep_conf.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <ep/ep_assert.h>
#include <ep/ep_stat.h>

//XXX hacks for non-libep settings
#define EP_SRC_ID(x)

/*
**  Initialization
*/

EP_STAT		ep_lib_init(uint32_t flags);

#define EP_LIB_USEPTHREADS	0x00000001	// turn on pthreads support

// the versions from ep_mem.h give you a bit more
void	*ep_mem_malloc(size_t);
void	*ep_mem_zalloc(size_t);
char	*ep_mem_strdup(const char *);
void	ep_mem_free(void *);

#define EP_GEN_DEADBEEF		((void *) 0xDEADBEEF)
// ideally this would check valid pointers for this architecture
#define EP_ASSERT_POINTER_VALID(p) \
				EP_ASSERT((p) != NULL && (p) != EP_GEN_DEADBEEF)

#define EP_UT_BITSET(bit, word)	(((bit) & (word)) != 0)

extern void		ep_adm_readparams(	// search for parameter files
				const char *name);	// name to search for
extern int		ep_adm_getintparam(	// get integer param value
				const char *name,	// name of param
				int def);		// default value
extern long		ep_adm_getlongparam(	// get long param value
				const char *name,	// name of param
				long def);		// default value
extern bool		ep_adm_getboolparam(	// get boolean param value
				const char *name,	// name of param
				bool def);		// default value
extern const char	*ep_adm_getstrparam(	// get string param value
				const char *name,	// name of param
				const char *def);	// default value

extern FILE		*ep_fopensmem(		// open a static memory buffer
				void *buf,		// buffer
				size_t bufsz,		// size of buffer
				const char *mode);	// mode, e.g., r, w
extern size_t		ep_fread_unlocked(	// unlocked version of fread
				void *buf,		// buffer area
				size_t sz,		// size of one item
				size_t n,		// number of items
				FILE *fp);		// file to read
extern size_t		ep_fwrite_unlocked(	// unlocked version of fwrite
				void *buf,		// buffer area
				size_t sz,		// size of one item
				size_t n,		// number of items
				FILE *fp);		// file to write

#endif // _EP_H_
