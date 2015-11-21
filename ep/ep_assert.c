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

#include <ep.h>
#include <ep_assert.h>
#include <ep_stat.h>
#include <ep_string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

EP_SRC_ID("@(#)$Id: ep_assert.c 286 2014-04-29 18:15:22Z eric $");

/***********************************************************************
**
**  EP_ASSERT_FAILURE -- internal routine to raise an assertion failure
**
**	Parameters:
**		expr -- what was the actual text of the assertion
**			expression
**		type -- the assertion type -- require, ensure, etc.
**		file -- which file contained the assertion
**		line -- which line was it on
**
**	Returns:
**		never
*/

static int	EpAssertNesting = 0;	// XXX-NOT-THREAD-SAFE
					//  (should be thread-private)
static void	(*EpAbortFunc)(void) = 0;

void
ep_assert_failure(
	const char *expr,
	const char *type,
	const char *file,
	int line)
{
	if (EpAssertNesting++ > 0)
		ep_assert_abort("Nested assertion failure");

	// should log something here
//	ep_log_propl(NULL, EP_STAT_SEV_ABORT, "Assert Failure",
//			"file",		file,
//			"line",		buf,
//			"type",		type,
//			"expr",		expr,
//			NULL,	NULL);

	fprintf(stderr, "%s%sAssertion failed at %s:%d: %s:\n\t%s%s\n",
			EpVid->vidfgcyan, EpVid->vidbgred,
			file, line, type, expr,
			EpVid->vidnorm);
	abort();
	/*NOTREACHED*/
}

/*
**  EP_ASSERT_ABORT --- force an abort
**
**	XXX should be in unix-dependent file (probably posix_io.c)
*/

void
ep_assert_abort(const char *msg)
{
	const char *msg0 = "!!!ABORT: ";
	char msg1[40];
	const char *msg2 = ": ";

	// give the application an opportunity to do something else
	if (EpAbortFunc != NULL)
		(*EpAbortFunc)();

	// no?  OK then, let's bail out near line 1
	strerror_r(errno, msg1, sizeof msg1);
	if (write(2, msg0, strlen(msg0)) <= 0 ||
		write(2, msg1, strlen(msg1)) <= 0 ||
		write(2, msg2, strlen(msg2)) <= 0 ||
		write(2, msg, strlen(msg)) <= 0 ||
		write(2, "\n", 1) <= 0)
	{
		// um, not much to do here; we just did this to make GCC happy
		abort();
	}
	abort();

	// if abort fails, what to do, what to do?
	// :XXX: UNIX specific
	for (;;)
	{
		exit(255);
		_exit(255);
	}
}
