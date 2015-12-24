/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**  ----- END LICENSE BLOCK -----
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
			: ep_assert_failure("require", __FILE__, __LINE__, \
					"%s", #e))

#define EP_ASSERT_ENSURE(e)			\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure("ensure", __FILE__, __LINE__, \
					"%s", #e))

#define EP_ASSERT_INSIST(e)			\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure("insist", __FILE__, __LINE__, \
					"%s", #e))

#define EP_ASSERT_INVARIANT(e)			\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure("invariant", __FILE__, __LINE__, \
					"%s", #e))

#define EP_ASSERT(e)				\
		((e)				\
			? ((void) 0)		\
			: ep_assert_failure("assert", __FILE__, __LINE__, \
					"%s", #e))

#define EP_ASSERT_FAILURE(...)		\
			ep_assert_failure("failure", __FILE__, __LINE__, \
					__VA_ARGS__)

// called if the assertion failed
extern void	ep_assert_failure(
			const char *type,
			const char *file,
			int line,
			const char *msg,
			...)
			__attribute__ ((noreturn));

// called if ep_assert_failure was rude enough to return
extern void	ep_assert_abort(const char *msg)
			__attribute__ ((noreturn));

#endif /*_EP_ASSERT_H_*/
