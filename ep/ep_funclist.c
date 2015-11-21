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
	ep_thr_mutex_init(&flp->mutex, EP_THR_MUTEX_DEFAULT);

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
