/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_app.c 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_string.h>
#include <ep_app.h>
#include <stdlib.h>

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
#elif _GNU_SOURCE
	extern char *program_invocation_short_name;
	return program_invocation_short_name;
#else
	return NULL;
# endif
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
	const char *progname;

	va_start(av, fmt);
	fprintf(stderr, "%sAPPLICATION WARNING:%s ",
		EpVid->vidinv,
		EpVid->vidnorm);
	if ((progname = ep_app_getprogname()) != NULL)
		fprintf(stderr, "%s: ", progname);
	if (fmt != NULL)
		vfprintf(stderr, fmt, av);
	else
		fprintf(stderr, "unknown warning");
	fprintf(stderr, "\n");
	va_end(av);
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_ERROR -- print an error message
//
//	Parameters:
//		fmt -- format for a message
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
	const char *progname;

	va_start(av, fmt);
	fprintf(stderr, "%sAPPLICATION ERROR:%s ",
		EpVid->vidinv,
		EpVid->vidnorm);
	if ((progname = ep_app_getprogname()) != NULL)
		fprintf(stderr, "%s: ", progname);
	if (fmt != NULL)
		vfprintf(stderr, fmt, av);
	else
		fprintf(stderr, "unknown error");
	fprintf(stderr, "\n");
	va_end(av);
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
	const char *progname;

	va_start(av, fmt);
	fprintf(stderr, "%sAPPLICATION ABORT:%s ",
		EpVid->vidinv,
		EpVid->vidnorm);
	if ((progname = ep_app_getprogname()) != NULL)
		fprintf(stderr, "%s: ", progname);
	if (fmt != NULL)
		vfprintf(stderr, fmt, av);
	else
		fprintf(stderr, "exiting");
	fprintf(stderr, "\n");
	va_end(av);

	ep_assert_abort("Application Abort");
	/*NOTREACHED*/
}
