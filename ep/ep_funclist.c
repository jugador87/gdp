/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2010, Eric P. Allman.  All rights reserved.
**	$Id: ep_funclist.c 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

/***********************************************************************
**
**  FUNCTION LISTS
**
**	Used anywhere that you might need a dynamic list of functions
**
***********************************************************************/

#include <ep.h>
#include <ep_funclist.h>
#include <ep_rpool.h>
#include <ep_stat.h>
#include <ep_thr.h>
#include <ep_assert.h>

EP_SRC_ID("@(#)$Id: ep_funclist.c 286 2014-04-29 18:15:22Z eric $");

/**************************  BEGIN PRIVATE  **************************/

// per-entry structure
struct fseg
{
	struct fseg	*next;			// next in list
	void		(*func)(void *);	// the function to call
	void		*arg;			// the argument to that func
};

// per-list structure
struct EP_FUNCLIST
{
	// administrative....
	const char	*name;			// for debugging
	uint32_t	flags;			// flag bits, see below
	EP_RPOOL	*rpool;			// resource pool
	EP_THR_MUTEX	mutex;			// locking node

	// memory for actual functions
	struct fseg	*list;			// the actual list
};

// flags bits
//  none at this time

/***************************  END PRIVATE  ***************************/



EP_FUNCLIST *
ep_funclist_new(
	const char *name)
{
	uint32_t flags = 0;
	EP_FUNCLIST *flp;
	EP_RPOOL *rp;

	if (name == NULL)
		name = "<funclist>";

	rp = ep_rpool_new(name, (size_t) 0);
	flp = ep_rpool_zalloc(rp, sizeof *flp);

	flp->flags = flags;
	flp->rpool = rp;
	ep_thr_mutex_init(&flp->mutex);

	return flp;
}

void    
ep_funclist_free(EP_FUNCLIST *flp)
{
	EP_ASSERT_POINTER_VALID(flp);
	ep_thr_mutex_destroy(&flp->mutex);
	ep_rpool_free(flp->rpool);	// also frees flp
}

void 
ep_funclist_push(EP_FUNCLIST *flp,
	void (*func)(void *),
	void *arg)
{
	struct fseg *fsp;

	EP_ASSERT_POINTER_VALID(flp);
	ep_thr_mutex_lock(&flp->mutex);

	// allocate a new function block and fill it
	fsp = ep_rpool_malloc(flp->rpool, sizeof *fsp);
	fsp->func = func;
	fsp->arg = arg;

	// link it onto the list
	fsp->next = flp->list;
	flp->list = fsp;

	ep_thr_mutex_unlock(&flp->mutex);
}

#if 0

void
ep_funclist_pop(EP_FUNCLIST *flp)
{
	struct fseg *fsp;

	EP_ASSERT_POINTER_VALID(flp);

	//  XXX  Is this worth anything?
	//  XXX  If so, implement it
}

void
ep_funclist_clear(EP_FUNCLIST *flp)
{
	struct fseg *fsp;

	EP_ASSERT_POINTER_VALID(flp);

	//  XXX  Is this worth anything?
	//  XXX  If so, implement it
}

#endif /* 0 */

void 
ep_funclist_invoke(EP_FUNCLIST *flp)
{
	struct fseg *fsp;

	EP_ASSERT_POINTER_VALID(flp);

	//XXX possible deadlock if a called function tries to modify the list
	ep_thr_mutex_lock(&flp->mutex);
	for (fsp = flp->list; fsp != NULL; fsp = fsp->next)
	{
		(*fsp->func)(fsp->arg);
	}
	ep_thr_mutex_unlock(&flp->mutex);
}
