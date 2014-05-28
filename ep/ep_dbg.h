/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_dbg.h 283 2011-01-07 21:43:47Z eric $
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

int	__EpDbgCurGen;		// current generation

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
