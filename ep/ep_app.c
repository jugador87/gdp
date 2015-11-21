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
#include <ep_string.h>
#include <ep_app.h>
#include <stdlib.h>
#include <sys/errno.h>

EP_SRC_ID("@(#)$Id: ep_app.c 252 2008-09-16 21:24:42Z eric $");

////////////////////////////////////////////////////////////////////////
//
//  Application support
//
//	Just to make life easier for app writers
//
////////////////////////////////////////////////////////////////////////

const char *
ep_app_getprogname(void)
{
#if EP_OSCF_HAS_GETPROGNAME
	return getprogname();
#elif __linux__
	extern char *program_invocation_short_name;
	return program_invocation_short_name;
#else
	return NULL;
# endif
}



////////////////////////////////////////////////////////////////////////
//
//  PRINTMESSAGE -- helper routine for message printing
//

static void
printmessage(const char *tag, const char *fmt, va_list av)
{
	const char *progname;

	if ((progname = ep_app_getprogname()) != NULL)
		fprintf(stderr, "%s: ", progname);
	fprintf(stderr, "%s%s%s: ", EpVid->vidinv, tag, EpVid->vidnorm);
	if (fmt != NULL)
		vfprintf(stderr, fmt, av);
	else
		fprintf(stderr, "unknown %s", tag);
	if (errno != 0)
	{
		char nbuf[40];

		strerror_r(errno, nbuf, sizeof nbuf);
		fprintf(stderr, "\n\t(%s)", nbuf);
	}
	fprintf(stderr, "\n");
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_WARN -- print a warning message
//
//	Parameters:
//		fmt -- format for a message
//		... -- arguments
//
//	Returns:
//		never
//

void
ep_app_warn(
	const char *fmt,
	...)
{
	va_list av;

	va_start(av, fmt);
	printmessage("WARNING", fmt, av);
	va_end(av);
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_ERROR -- print an error message
//
//	Parameters:
//		fmt -- format for a message
//			If NULL it prints errno.
//		... -- arguments
//
//	Returns:
//		never
//

void
ep_app_error(
	const char *fmt,
	...)
{
	va_list av;

	va_start(av, fmt);
	printmessage("ERROR", fmt, av);
	va_end(av);
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_FATAL -- print a fatal error message and exit
//
//	Just uses a generic exit status.
//
//	Parameters:
//		fmt -- format for a message
//		... -- arguments
//
//	Returns:
//		never
//

void
ep_app_fatal(
	const char *fmt,
	...)
{
	va_list av;

	va_start(av, fmt);
	printmessage("FATAL", fmt, av);
	va_end(av);

	fprintf(stderr, "\t(exiting)\n");
	exit(1);
	/*NOTREACHED*/
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_ABORT -- print an error message and abort
//
//	Parameters:
//		fmt -- format for a message
//		... -- arguments
//
//	Returns:
//		never
//

void
ep_app_abort(
	const char *fmt,
	...)
{
	va_list av;

	va_start(av, fmt);
	printmessage("ABORT", fmt, av);
	va_end(av);

	fprintf(stderr, "\n\t(exiting)\n");
	abort();
	/*NOTREACHED*/
}
