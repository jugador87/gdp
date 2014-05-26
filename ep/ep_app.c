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
	ep_st_fprintf(EpStStderr, "%sAPPLICATION WARNING:%s ",
		EpVid->vidinv,
		EpVid->vidnorm);
	if ((progname = ep_app_getprogname()) != NULL)
		ep_st_fprintf(EpStStderr, "%s: ", progname);
	if (fmt != EP_NULL)
		ep_st_vprintf(EpStStderr, fmt, av);
	else
		ep_st_fprintf(EpStStderr, "unknown warning");
	ep_st_fprintf(EpStStderr, "\n");
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
	ep_st_fprintf(EpStStderr, "%sAPPLICATION ERROR:%s ",
		EpVid->vidinv,
		EpVid->vidnorm);
	if ((progname = ep_app_getprogname()) != NULL)
		ep_st_fprintf(EpStStderr, "%s: ", progname);
	if (fmt != EP_NULL)
		ep_st_vprintf(EpStStderr, fmt, av);
	else
		ep_st_fprintf(EpStStderr, "unknown error");
	ep_st_fprintf(EpStStderr, "\n");
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
	ep_st_fprintf(EpStStderr, "%sAPPLICATION ABORT:%s ",
		EpVid->vidinv,
		EpVid->vidnorm);
	if ((progname = ep_app_getprogname()) != NULL)
		ep_st_fprintf(EpStStderr, "%s: ", progname);
	if (fmt != EP_NULL)
		ep_st_vprintf(EpStStderr, fmt, av);
	else
		ep_st_fprintf(EpStStderr, "exiting");
	ep_st_fprintf(EpStStderr, "\n");
	va_end(av);

	ep_assert_abort("Application Abort");
	/*NOTREACHED*/
}
