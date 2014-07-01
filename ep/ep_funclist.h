/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2010, Eric P. Allman.  All rights reserved.
**	$Id: ep_funclist.h 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

////////////////////////////////////////////////////////////////////////
//
//  FUNCTION LISTS
//
////////////////////////////////////////////////////////////////////////

#ifndef _EP_FUNCLIST_H_
#define _EP_FUNCLIST_H_

// the function list itself (contents are private)
typedef struct EP_FUNCLIST	EP_FUNCLIST;

// the type of a function passed to the funclist
typedef void		(EP_FUNCLIST_FUNC)(void *arg);

// the funclist primitives
extern EP_FUNCLIST	*ep_funclist_new(
				const char *name);
extern void		ep_funclist_free(
				EP_FUNCLIST *flp);
extern void		ep_funclist_push(
				EP_FUNCLIST *flp,
				EP_FUNCLIST_FUNC *func,
				void *arg);
extern void		ep_funclist_invoke(
				EP_FUNCLIST *flp);

#endif //_EP_FUNCLIST_H_
