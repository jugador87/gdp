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

#define EP_ASSERT_FAILURE(m)			\
			ep_assert_failure(m, "failure", __FILE__, __LINE__)

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
