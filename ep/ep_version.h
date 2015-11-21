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
#define EP_VERSION_MINOR	2
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
