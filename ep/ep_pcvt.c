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

EP_SRC_ID("@(#)$Id: ep_pcvt.c 252 2008-09-16 21:24:42Z eric $");

/*
**  PRINT CONVERSION ROUTINES
**
**	These are conversions from various formats to string, intended
**	for short-term use.  In all cases, the caller passes in a
**	buffer in which to store the results.  The value will be
**	trimmed as necessary to fit that area.
*/

const char *
ep_pcvt_str(
	char *obuf,
	size_t osize,
	const char *val)
{
	int vl;

	if (val == NULL)
	{
		strlcpy(obuf, "<NULL>", osize);
		return obuf;
	}

	vl = strlen(val);
	if (vl < osize)
		return val;
	strncpy(obuf, val, osize - 7);
	strlcat(obuf, "...", osize);
	strlcat(obuf, &val[vl - 3], osize);
	return obuf;
}


const char *
ep_pcvt_int(
	char *obuf,
	size_t osize,
	int base,
	long val)
{
	char *fmtstr;

	switch (base)
	{
	  case 8:
	  case -8:
		fmtstr = "%o";
		break;

	  case 16:
	  case -16:
		fmtstr = "%X";
		break;

	  case 10:
	  default:
		fmtstr = "%l";
		break;

	  case -10:
		fmtstr = "%ul";
		break;
	}

	snprintf(obuf, osize, fmtstr, val);
	return obuf;
}
