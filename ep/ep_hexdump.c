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
#include <ep_hexdump.h>
#include <ep_string.h>

#include <ctype.h>
#include <stdio.h>

void
ep_hexdump(const void *bufp, size_t buflen, FILE *fp,
		int format, size_t offset)
{
	size_t bufleft = buflen;
	const uint8_t *b = bufp;
	const size_t width = 16;

	flockfile(fp);			// to make threads happy
	while (bufleft > 0)
	{
		int lim = bufleft;
		int i;
		int shift;

		shift = offset % width;
		if (lim > width)
			lim = width;
		if (lim > (width - shift))
			lim = width - shift;
		fprintf(fp, "%08zx", offset);
		if (shift != 0)
			fprintf(fp, "%*s", shift * 3, "");
		for (i = 0; i < lim; i++)
			fprintf(fp, " %02x", b[i]);
		if (EP_UT_BITSET(EP_HEXDUMP_ASCII, format))
		{
			fprintf(fp, "\n        ");
			if (shift != 0)
				fprintf(fp, "%*s", shift * 3, "");
			for (i = 0; i < lim; i++)
			{
				if (isprint(b[i]))
					fprintf(fp, " %c ", b[i]);
				else
					fprintf(fp, " %s ", EpChar->unprintable);
			}
		}
		fprintf(fp, "\n");
		b += lim;
		bufleft -= lim;
		offset += lim;
	}
	funlockfile(fp);
}
