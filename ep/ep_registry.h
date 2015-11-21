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

/***********************************************************************
**
**  VENDOR REGISTRY
**
**	Get your vendor registrations here....
**
**	There are 2048 vendor registries possible, allocated as shown
**	below (all numbers in hex):
**
**	  000		reserved for generic status codes (e.g. "OK")
**	  001		single use (one application, conflicts expected)
**	  002-07F	available for local, unregistered use (e.g.,
**			separate modules within one large application)
**	  080-0FF	available for internal corporate registry
**			(not registered here; may conflict between
**			organizations but not within an organization)
**	  100		eplib itself
**	  101-6FF	available for global registry (this file)
**	  700-7FE	reserved
**	  7FF		must not be used: represents "all registries"
**			(EP_STAT_REGISTRY_ALL)
**
**	If you are not producing libraries for external use (i.e., where
**	conflicts might actually appear), please use the range from
**	0002-01FF.
**
**	To reserve a code from the global registry, send mail to
**	eplib-registry (AT) neophilic.com.
**
***********************************************************************/

#define EP_REGISTRY_GENERIC		0x000	// reserved for generic codes
#define EP_REGISTRY_USER		0x001	// reserved for user apps

#define EP_REGISTRY_EPLIB		0x100	// for eplib
#define EP_REGISTRY_NEOPHILIC		0x101	// Neophilic Systems
#define EP_REGISTRY_SENDMAIL		0x102	// Sendmail, Inc.
#define EP_REGISTRY_UCB			0x103	// U.C. Berkeley
