/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_xlate.h 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

#ifndef _EP_XLATE_H_
#define _EP_XLATE_H_


#define EP_XLATE_PERCENT	0x00000001	// translate %xx like SMTP/URLs
#define EP_XLATE_BSLASH		0x00000002	// translate \ like C
#define EP_XLATE_AMPER		0x00000004	// translate &name; like HTML
#define EP_XLATE_PLUS		0x00000008	// translate +xx like DNSs
#define EP_XLATE_EQUAL		0x00000010	// translate =xx like Q-P
#define EP_XLATE_8BIT		0x00000100	// encode 8-bit chars
#define EP_XLATE_NPRINT		0x00000200	// encode non-printable chars

extern int	ep_xlate_in(
			const char *in,
			unsigned char *out,
			size_t olen,
			char stopchar,
			uint32_t how);

extern int	ep_xlate_out(
			const char *in,
			size_t ilen,
			EP_STREAM *osp,
			const char *forbid,
			uint32_t how);
#endif // _EP_XLATE_H_
