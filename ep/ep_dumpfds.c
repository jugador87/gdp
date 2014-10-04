/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2014, Eric P. Allman.  All rights reserved.
***********************************************************************/

#include <ep.h>
#include <sys/stat.h>
#include <sys/errno.h>

/*
**  EP_DUMPFDS --- show all open file descriptors
**
**  	Right now doesn't show much....
*/

void
ep_dumpfds(FILE *fp)
{
	long maxfds = sysconf(_SC_OPEN_MAX);
	int i;
	int j;

	fprintf(fp, "\n*** Open File Descriptors:\n");
	for (i = j = 0; i < maxfds; i++)
	{
		struct stat sbuf;

		if (fstat(i, &sbuf) < 0)
			continue;
		if (++j > 10)
		{
			fprintf(fp, "\n");
			j = 0;
		}
		fprintf(fp, " %3d", i);
		if ((sbuf.st_mode & S_IFMT) == S_IFSOCK)
			fprintf(fp, "s");
		else
			fprintf(fp, " ");
	}
	fprintf(fp, "\n");
	errno = 0;
}
