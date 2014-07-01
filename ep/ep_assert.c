/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2014, Eric P. Allman.  All rights reserved.
**	$Id: ep_assert.c 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_stat.h>
#include <ep_assert.h>
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

	fprintf(stderr, "Assertion failed at %s:%d: %s: %s\n",
			file, line, type, expr);
	abort();
	/*NOTREACHED*/
}

// :XXX: should be in unix-dependent file (probably posix_io.c)

void
ep_assert_abort(const char *msg)
{
	const char *msg0 = "!!!ABORT: ";
	const char *msg1;
	const char *msg2 = ": ";

	// give the application an opportunity to do something else
	if (EpAbortFunc != NULL)
		(*EpAbortFunc)();

	// no?  OK then, let's bail out near line 1
	msg1 = strerror(errno);
	write(2, msg0, strlen(msg0));
	write(2, msg1, strlen(msg1));
	write(2, msg2, strlen(msg2));
	write(2, msg, strlen(msg));
	write(2, "\n", 1);
	abort();

	// if abort fails, what to do, what to do?
	// :XXX: UNIX specific
	for (;;)
	{
		exit(255);
		_exit(255);
	}
}
