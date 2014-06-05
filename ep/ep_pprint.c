/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_st_pprint.c 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_stat.h>
#include <ctype.h>

EP_SRC_ID("@(#)$Id: ep_st_pprint.c 252 2008-09-16 21:24:42Z eric $");

/***********************************************************************
**
**  EP_ST_PPRINT -- positional print
**
**	Interpret %N in fmt (where N is an number) as interpolating
**	the Nth argument.
**
**	Parameters:
**		fp -- the stream to print to
**		fmt -- the format string
**		argc -- the number of arguments
**		argv -- the actual arguments
**
**	Returns:
**		none
*/

void
ep_pprint(FILE *fp,
	const char *fmt,
	int argc,
	const char *const *argv)
{
	const char *ap;
	char c;
	int i;

	while ((c = *fmt++) != '\0')
	{
		// if it's not a percent, just copy through
		if (c != '%')
		{
			putc(c, fp);
			continue;
		}

		// if it is, look at the next byte
		if ((c = *fmt++) == '\0')
		{
			// hack for percent at end of string
			putc('%', fp);
			break;
		}

		if (!isdigit(c))
		{
			// bogus character, unless %%
			putc('%', fp);
			if (c != '%')
				putc(c, fp);
			continue;
		}

		// interpolate Nth argument
		i = c - '0';
		if (i <= 0 || i > argc)
			ap = "(unknown)";
		else if (argv[i - 1] == NULL)
			ap = "(null)";
		else
			ap = argv[i - 1];

		while ((c = *ap++) != '\0')
			putc(c, fp);
	}
}