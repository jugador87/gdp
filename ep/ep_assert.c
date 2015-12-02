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

#include <ep.h>
#include <ep_assert.h>
#include <ep_stat.h>
#include <ep_string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


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
