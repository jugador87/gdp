/* vim: set ai sw=8 sts=8 :*/

/*
**  Open up a stream backed by (static) memory.
**
** 	This is a wrapper around funopen (on BSD and MacOS) or
** 	fopencookie (on Linux).  These two routimes do substantially
** 	the same thing.
*/

#if __FreeBSD__ || __APPLE__
#   define IORESULT_T	int
#   define IOBLOCK_T	int
#elif __linux__
#   define IORESULT_T	ssize_t
#   define IOBLOCK_T	size_t
#   define _GNU_SOURCE	1
#   include <libio.h>
#else
#   error Cannot determine use of funopen vs fopencookie
#endif

#include <ep.h>
#include <string.h>
#include <stdio.h>

struct meminfo
{
	char *bufb;		// base of buffer
	size_t bufs;		// size of buffer
	size_t bufx;		// index of current read/write pointer
};

static IORESULT_T
memread(void *cookie, char *buf, IOBLOCK_T size)
{
	struct meminfo *minf = cookie;
	size_t l = minf->bufs - minf->bufx;

	if (l > size)
		l = size;
	if (l > 0)
		memcpy(buf, minf->bufb + minf->bufx, l);
	minf->bufx += l;
	return l;
}

static IORESULT_T
memwrite(void *cookie, const char *buf, IOBLOCK_T size)
{
	struct meminfo *minf = cookie;
	size_t l = minf->bufs - minf->bufx;

	if (l > size)
		l = size;
	if (l > 0)
		memcpy(minf->bufb + minf->bufx, buf, l);
	minf->bufx += l;
	return l;
}

static int
memclose(void *cookie)
{
	ep_mem_free(cookie);
	return 0;
}


FILE *
ep_fopensmem(void *buf,
	size_t size,
	const char *mode)
{
	struct meminfo *minf;

	minf = ep_mem_zalloc(sizeof *minf);
	if (minf == NULL)
		return NULL;
	minf->bufb = buf;
	minf->bufs = size;
	minf->bufx = 0;

#if __FreeBSD__ || __APPLE__
	{
		// BSD/MacOS
		return funopen(minf, &memread, &memwrite, NULL, &memclose);
	}
#elif __linux__
	{
		// Linux
		cookie_io_functions_t iof =
		{
			&memread,
			&memwrite,
			NULL,
			&memclose
		};
		return fopencookie(minf, mode, iof);
	}
#endif
}
