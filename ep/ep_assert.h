/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_assert.h 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

////////////////////////////////////////////////////////////////////////
//
//  ASSERTIONS
//
//	There are several types of assertions.  This is all based on
//	the ISC library.
//
//	Semantics are as follows:
//	    require	Assertions about input to routines
//	    ensure	Assertions about output from routines
//	    invariant	Assertions about loop invariants
//	    insist	General otherwise unclassified stuff
//
//	There's also an "EP_ASSERT" for back compatibility
//
////////////////////////////////////////////////////////////////////////

#ifndef _EP_ASSERT_H_
#define _EP_ASSERT_H_

// assert that an expression must be true
#define EP_ASSERT_REQUIRE(e)			\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure(#e, "require", __FILE__, __LINE__))

#define EP_ASSERT_ENSURE(e)			\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure(#e, "ensure", __FILE__, __LINE__))

#define EP_ASSERT_INSIST(e)			\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure(#e, "insist", __FILE__, __LINE__))

#define EP_ASSERT_INVARIANT(e)			\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure(#e, "invariant", __FILE__, __LINE__))

#define EP_ASSERT(e)				\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure(#e, "assert", __FILE__, __LINE__))

// called if the assertion failed
extern void	ep_assert_failure(
			const char *expr,
			const char *type,
			const char *file,
			int line)
			__attribute__ ((noreturn));

// called if ep_assert_failure was rude enough to return
extern void	ep_assert_abort(const char *msg)
			__attribute__ ((noreturn));

#endif /*_EP_ASSERT_H_*/
