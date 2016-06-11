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
**		type -- the assertion type -- require, ensure, etc.
**		file -- which file contained the assertion
**		line -- which line was it on
**		msg -- the message to print (printf format)
**		... -- arguments
**
**	Returns:
**		never
*/

void	(*EpAbortFunc)(void) = NULL;

void
ep_assert_failure(
	const char *type,
	const char *file,
	int line,
	const char *msg,
	...)
{
	va_list av;

	// log something here?

	flockfile(stderr);
	fprintf(stderr, "%s%sAssertion failed at %s:%d: %s:\n\t",
			EpVid->vidfgcyan, EpVid->vidbgred,
			file, line, type);
	va_start(av, msg);
	vfprintf(stderr, msg, av);
	va_end(av);
	fprintf(stderr, "%s\n", EpVid->vidnorm);
	funlockfile(stderr);

	// give the application an opportunity to do something else
	if (EpAbortFunc != NULL)
	{
		void (*abortfunc)(void) = EpAbortFunc;
		EpAbortFunc = NULL;
		(*abortfunc)();
	}

	abort();
	/*NOTREACHED*/
}
