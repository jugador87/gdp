/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_version.h 291 2014-05-11 21:42:53Z eric $
***********************************************************************/

////////////////////////////////////////////////////////////////////////
//
//  VERSION NUMBER
//
//	Patches shouldn't include new functionality, just bug fixes.
//
//	Used by ep_gen.h to make string and numeric versions.
//
////////////////////////////////////////////////////////////////////////

#ifndef _EP_VERSION_H_
#define _EP_VERSION_H_

#include <ep/ep.h>

// change these as necessary
#define EP_VERSION_MAJOR	2
#define EP_VERSION_MINOR	0
#define EP_VERSION_PATCH	0

#define __EP_STRING(x)		#x
#define __EP_VER_CONCAT(a, b, c)					\
		__EP_STRING(a) "." __EP_STRING(b) "." __EP_STRING(c)
#define EP_VER_STRING							\
		__EP_VER_CONCAT(EP_VERSION_MAJOR,			\
				EP_VERSION_MINOR,			\
				EP_VERSION_PATCH)

#define EP_VERSION	((EP_VERSION_MAJOR << 24) |			\
			 (EP_VERSION_MINOR << 16) |			\
			 (EP_VERSION_PATCH      ))

extern const char	EpVersion[];

#endif
