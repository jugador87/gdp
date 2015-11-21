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

/*
**  EP_FREAD_UNLOCKED --- unlocked file read
**
**	This is needed only because there is no Posix standard
**	fread_unlocked.
**
**	Be sure you use flockfile before calling this if you will
**	ever use this in a threaded environment!
*/

#include <ep.h>
#include <stdio.h>

size_t
ep_fread_unlocked(void *ptr, size_t size, size_t n, FILE *fp)
{
	size_t nbytes = size * n;
	char *b = ptr;
	int i;
	int c;

	if (nbytes == 0)
		return 0;
	for (i = 0; i < nbytes; i++)
	{
		c = getc_unlocked(fp);
		if (c == EOF)
			break;
		*b++ = c;
	}
	return i / size;
}


/*
**  EP_FWRITE_UNLOCKED --- unlocked file write
**
**	This is only needed because there is no Posix standard
**	fwrite_unlocked.
**
**	Be sure you use flockfile before calling this if you will
**	ever use this in a threaded environment!
*/

size_t
ep_fwrite_unlocked(void *ptr, size_t size, size_t n, FILE *fp)
{
	size_t nbytes = size * n;
	char *b = ptr;
	int i;

	if (nbytes == 0)
		return 0;
	for (i = 0; i < nbytes; i++)
	{
		if (putc_unlocked(*b++, fp) == EOF)
			break;
	}
	return i / size;
}
