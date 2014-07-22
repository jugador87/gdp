/* vim: set ai sw=8 sts=8 :*/

/*
**  EP_FREAD_UNLOCKED --- unlocked file read
**
**  	This is needed only because there is no Posix standard
**  	fread_unlocked.
**
**  	Be sure you use flockfile before calling this if you will
**  	ever use this in a threaded environment!
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
**  	This is only needed because there is no Posix standard
**  	fwrite_unlocked.
**
**  	Be sure you use flockfile before calling this if you will
**  	ever use this in a threaded environment!
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
