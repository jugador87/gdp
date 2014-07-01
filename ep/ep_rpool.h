/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2010, Eric P. Allman.  All rights reserved.
**	$Id: ep_rpool.h 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

////////////////////////////////////////////////////////////////////////
//
//  MEMORY ALLOCATION: RESOURCE POOLS
//
////////////////////////////////////////////////////////////////////////

#ifndef _EP_RPOOL_H_
#define _EP_RPOOL_H_

# include <ep/ep_mem.h>

typedef struct EP_RPOOL		EP_RPOOL;

typedef void			(EP_RPOOL_FREEFUNC)(void *);

extern EP_RPOOL *ep_rpool_new(			// create new pool
			const char *name,		// name (debugging)
			const size_t quantum);		// allocation size
extern void	ep_rpool_free(			// free everything in pool
			EP_RPOOL *rp);			// rpool to free
extern void	*ep_rpool_malloc(		// allocate memory
			EP_RPOOL *rp,			// source rpool
			size_t nbytes);			// bytes to get
extern void	*ep_rpool_zalloc(		// allocate zeroed memory
			EP_RPOOL *rp,			// source rpool
			size_t nbytes);			// bytes to get
extern void	*ep_rpool_realloc(		// change allocation size
			EP_RPOOL *rp,			// source rpool
			void *emem,			// old memory
			size_t oldsize,			// size of old memory
			size_t newsize);		// size of new memory
extern char	*ep_rpool_istrdup(		// save string in rpool
			EP_RPOOL *rp,			// rpool to use
			const char *s,			// str to copy
			int slen,			// max length of s
			uint32_t flags,			// action modifiers
			const char *file,		// dbg: file name
			int line);			// dbg: line number
extern void	ep_rpool_mfree(			// free block of memory
			EP_RPOOL *rp,			// owner rpool
			void *p);			// space to free
//extern void	ep_rpool_attach(		// attach free func to rpool
//			EP_RPOOL *rp,			// target rpool
//			EP_RPOOL_FREEFUNC *freefunc,	// free function
//			void *arg);			// arg to free func
extern void	*ep_rpool_ialloc(		// allocate memory from pool
			EP_RPOOL *rp,			// source pool
			size_t nbytes,			// bytes to get
			uint32_t flags,			// action modifiers
			const char *file,		// dbg: file name
			int line);			// dbg: file number

# define ep_rpool_malloc(rp, size) \
				ep_rpool_ialloc(rp, size, 0, \
					_EP_MEM_FILE_LINE_)
# define ep_rpool_strdup(rp, s)	ep_rpool_istrdup(rp, s, -1, 0, \
					_EP_MEM_FILE_LINE_)
# define ep_rpool_strndup(rp, s) \
				ep_rpool_istrdup(rp, s, l, 0, \
					_EP_MEM_FILE_LINE_)

#endif //_EP_RPOOL_H_
