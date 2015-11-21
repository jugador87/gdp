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

#ifndef _EP_STRING_H_
#define _EP_STRING_H_

#include <string.h>

// these really don't belong here -- maybe ep_video.h

// video sequences for terminals
extern struct epVidSequences
{
	const char	*vidnorm;	// set video to normal
	const char	*vidbold;	// set video to bold
	const char	*vidfaint;	// set video to faint
	const char	*vidstout;	// set video to "standout"
	const char	*viduline;	// set video to underline
	const char	*vidblink;	// set video to blink
	const char	*vidinv;	// set video to invert
	const char	*vidfgblack;	// set foreground black
	const char	*vidfgred;	// set foreground red
	const char	*vidfggreen;	// set foreground green
	const char	*vidfgyellow;	// set foreground yellow
	const char	*vidfgblue;	// set foreground blue
	const char	*vidfgmagenta;	// set foreground magenta
	const char	*vidfgcyan;	// set foreground cyan
	const char	*vidfgwhite;	// set foreground white
	const char	*vidbgblack;	// set background black
	const char	*vidbgred;	// set background red
	const char	*vidbggreen;	// set background green
	const char	*vidbgyellow;	// set background yellow
	const char	*vidbgblue;	// set background blue
	const char	*vidbgmagenta;	// set background magenta
	const char	*vidbgcyan;	// set background cyan
	const char	*vidbgwhite;	// set background white
} *EpVid;

// target video treatment, corresponding to above
//  (currently used only in ep_st_printf for %#c and %#s printing)
#define EP_VID_NORM	0
#define EP_VID_BOLD	0x00000001
#define EP_VID_FAINT	0x00000002
#define EP_VID_STOUT	0x00000004
#define EP_VID_ULINE	0x00000008
#define EP_VID_BLINK	0x00000010
#define EP_VID_INV	0x00000020

extern EP_STAT	ep_str_vid_set(const char *);


// character sequences (e.g., ASCII, ISO-8859-1, UTF-8)
extern struct epCharSequences
{
	const char	*lquote;	// left quote or '<<' character
	const char	*rquote;	// right quote or '>>' character
	const char	*copyright;	// copyright symbol
	const char	*degree;	// degree symbol
	const char	*micro;		// micro symbol
	const char	*plusminus;	// +/- symbol
	const char	*times;		// times symbol
	const char	*divide;	// divide symbol
	const char	*null;		// null symbol
	const char	*notequal;	// not equal symbol
	const char	*unprintable;	// non-printable character
	const char	*paragraph;	// paragraph symbol
	const char	*section;	// section symbol
	const char	*notsign;	// "not" symbol
	const char	*infinity;	// infinity symbol
} *EpChar;

extern EP_STAT	ep_str_char_set(const char *);

// function to adjust alternate printing
extern uint32_t	(*EpStPrintfAltprFunc)(int *, char *, char *);


#ifndef EP_OSCF_HAS_STRLCPY
# define EP_OSCF_HAS_STRLCPY	1
#endif

#if !EP_OSCF_HAS_STRLCPY
extern size_t	strlcpy(char *dst, const char *src, size_t n);
extern size_t	strlcat(char *dst, const char *src, size_t n);
#endif

#define ep_str_lcpy(dst, src, siz)	strlcpy(dst, src, siz)
#define ep_str_lcat(dst, src, siz)	strlcat(dst, src, siz)
#define ep_str_casecmp(s1, s2)		strcasecmp(s1, s2)
#define ep_str_ncasecmp(s1, s2, l)	strncasecmp(s1, s2, l)

extern size_t	ep_str_lcpyn(char *dst, size_t siz, ...);
extern size_t	ep_str_lcatn(char *dst, size_t siz, ...);

#endif /* _EP_STRING_H_ */
