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

#ifndef _EP_DEBUG_H_
#define _EP_DEBUG_H_

# include <stdarg.h>

/**************************  BEGIN PRIVATE  **************************/

// if the current generation is greater than that in the flag then some
// debug flag has been changed and the whole thing should be re-inited.

typedef struct EP_DBG	EP_DBG;

struct EP_DBG
{
	const char	*name;	// debug flag name
	int		level;	// current debug level
	const char	*desc;	// description
	int		gen;	// flag initialization generation
	EP_DBG		*next;	// initted flags, in case values change
};

extern int	__EpDbgCurGen;		// current generation

/***************************  END PRIVATE  ***************************/

// macros for use in applications
#define ep_dbg_level(f)		((f).gen == __EpDbgCurGen ?		\
				 (f).level :				\
				 ep_dbg_flaglevel(&f))
#define ep_dbg_test(f, l)	(ep_dbg_level(f) >= (l))

// support functions
extern int	ep_dbg_flaglevel(EP_DBG *f);

// creating a flag
#define EP_DBG_INIT(name, desc)						\
		{ name, -1, "@(#)$Debug: " name " = " desc " $", -1, NULL }

// initialization
extern void	ep_dbg_init(void);

// setting/fetching debug output file
extern void	ep_dbg_setfile(FILE *fp);
extern FILE	*ep_dbg_getfile(void);

// setting debug flags
extern void	ep_dbg_set(const char *s);
extern void	ep_dbg_setto(const char *pat, int lev);

// printing debug output (uses stddbg)
extern void EP_TYPE_PRINTFLIKE(1, 2)
		ep_dbg_printf(const char *fmt, ...);

// print only if flag set
#define ep_dbg_cprintf(f, l, ...)	{if (ep_dbg_test(f, l)) \
						ep_dbg_printf(__VA_ARGS__);}

// crackarg parsing
extern EP_STAT	epCavDebug(const char *vp, void *rp);


#endif /*_EP_DEBUG_H_*/
