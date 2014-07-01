/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_prflags.h 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

#ifndef _EP_PRFLAGS_H_
#define _EP_PRFLAGS_H_

typedef struct ep_prflags_desc
{
	uint32_t	bits;		// bits to compare against
	uint32_t	mask;		// mask against flagword
	const char	*name;		// printable name
} EP_PRFLAGS_DESC;

extern void	ep_prflags(
			uint32_t		flagword,
			const EP_PRFLAGS_DESC	*flagdesc,
			FILE			*outfile);

#endif // _EP_PRFLAGS_H_
