/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_app.h 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

#ifndef _EP_APP_H_
# define _EP_APP_H_
# include <ep/ep_conf.h>
# include <stdlib.h>

extern void EP_TYPE_PRINTFLIKE(1, 2)
		ep_app_warn(const char *fmt, ...);
extern void EP_TYPE_PRINTFLIKE(1, 2)
		ep_app_error(const char *fmt, ...);
extern void EP_TYPE_PRINTFLIKE(1, 2)
		ep_app_abort(const char *fmt, ...)
			__attribute__ ((noreturn));

extern const char	*ep_app_getprogname(void);

#endif //_EP_APP_H_
