/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2010, Eric P. Allman.  All rights reserved.
**	$Id: ep_pcvt.h 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

#ifndef _EP_PCVT_H_
#define _EP_PCVT_H_

extern char	*ep_pcvt_str(char *obuf, size_t osize, const char *val);
extern char	*ep_pcvt_int(char *obuf, size_t osize, int base, long val);

#endif // _EP_PCVT_H_
