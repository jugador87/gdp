/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_prflags.c 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_prflags.h>
#include <stdio.h>

EP_SRC_ID("@(#)$Id: ep_prflags.c 252 2008-09-16 21:24:42Z eric $");

/*
**  EP_PRFLAGS -- print out a flags word in a user-friendly manner
**
**	Parameters:
**		flagword -- the word of flags to print
**		flagdesc -- a vector of descriptors for those flags
**		out -- the output file on which to print them
**
**	Returns:
**		none.
*/

void
ep_prflags(
	uint32_t flagword,
	const EP_PRFLAGS_DESC *flagdesc,
	FILE *out)
{
	bool firsttime = true;

	if (flagword == 0)
	{
		fprintf(out, "0");
		return;
	}
	fprintf(out, "0x%x<", flagword);
	for (; flagdesc->name != NULL; flagdesc++)
	{
		if (((flagword ^ flagdesc->bits) & flagdesc->mask) != 0)
			continue;
		fprintf(out, "%s%s",
			firsttime ? "" : ",",
			flagdesc->name);
		firsttime = false;
	}
	fprintf(out, ">");
}
