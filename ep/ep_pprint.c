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
