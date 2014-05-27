/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_dbg.c 287 2014-04-29 18:18:23Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_dbg.h>
#include <ep_string.h>
#include <ep_assert.h>
#include <fnmatch.h>

EP_SRC_ID("@(#)$Id: ep_dbg.c 287 2014-04-29 18:18:23Z eric $");

typedef struct FLAGPAT	FLAGPAT;

struct FLAGPAT
{
	const char	*pat;		// flag pattern (text)
	int		lev;		// level to set the pattern
	FLAGPAT		*next;		// next in chain
};

static EP_MUTEX	FlagListMutex;
static FLAGPAT	*FlagList;
FILE		*EpStStddbg;


/*
**  EP_DBG_INIT -- initialize debug subsystem
*/

void
ep_dbg_init(void)
{
	if (EpStStddbg == NULL)
		ep_dbg_setstream(NULL);
	EP_ASSERT(EpStStddbg != NULL);
}

/*
**  EP_DBG_SET -- set a debugging flag
**
**	Should probably do more syntax checking here.
*/

void
ep_dbg_set(const char *fspec)
{
	const char *f = fspec;
	int i = 0;
	char pbuf[200];

	ep_dbg_init();

	if (f == NULL)
		return;
	while (*f != '\0' && *f != '=')
	{
		if (i <= sizeof pbuf - 1)
			pbuf[i++] = *f;
		f++;
	}
	pbuf[i] = '\0';
	if (*f == '=')
		f++;
	ep_dbg_setto(pbuf, strtol(f, NULL, 10));
}

/*
**  EP_DBG_SETTO -- set a debugging flag to a particular value
*/

void
ep_dbg_setto(const char *fpat,
	int lev)
{
	FLAGPAT *fp;
	int patlen = strlen(fpat) + 1;
	char *tpat;

	fp = ep_mem_malloc(sizeof *fp + patlen);
	tpat = ((char *) fp) + sizeof *fp;
	(void) strncpy(tpat, fpat, patlen);
	fp->pat = tpat;
	fp->lev = lev;

	// link to front of chain
	EP_MUTEX_LOCK(FlagListMutex);
	fp->next = FlagList;
	FlagList = fp;
	__EpDbgCurGen++;
	EP_MUTEX_UNLOCK(FlagListMutex);
}


/*
**  EP_DBG_FLAGLEVEL -- return flag level, initializing if necessary
**
**	This checks to see if the flag is specified; in any case it
**	updates the local value indicator.
**
**	Parameters:
**		flag -- the flag to be tested
**
**	Returns:
**		The current flag level
*/

int
ep_dbg_flaglevel(EP_DBG *flag)
{
	FLAGPAT *fp;

	EP_MUTEX_LOCK(FlagListMutex);
	flag->gen = __EpDbgCurGen;
	for (fp = FlagList; fp != NULL; fp = fp->next)
	{
		if (fnmatch(fp->pat, flag->name, 0))
			break;
	}
	if (fp == NULL)
		flag->level = 0;
	else
		flag->level = fp->lev;
	EP_MUTEX_UNLOCK(FlagListMutex);

	return flag->level;
}


/*
**  EP_DBG_CPRINTF -- print debug info conditionally on flag match
**
**	Parameters:
**		flag -- the flag to test
**		level -- the level that flag must be at (or higher)
**		fmt -- the message to print
**		... -- parameters to that fmt
**
**	Returns:
**		none
*/

bool
ep_dbg_cprintf(
	EP_DBG *flag,
	int level,
	const char *fmt,
	...)
{
	if (EpStStddbg == NULL)
		ep_dbg_init();

	if (ep_dbg_test(flag, level))
	{
		va_list av;

		fprintf(EpStStddbg, "%s", EpVid->vidfgyellow);
		va_start(av, fmt);
		vfprintf(EpStStddbg, fmt, av);
		va_end(av);
		fprintf(EpStStddbg, "%s", EpVid->vidnorm);
		return true;
	}
	return false;
}


/*
**  EP_DBG_PRINTF -- print debug information
**
**	Parameters:
**		fmt -- the message to print
**		... -- parameters to that fmt
**
**	Returns:
**		none.
*/

void
ep_dbg_printf(const char *fmt, ...)
{
	va_list av;

	if (EpStStddbg == NULL)
		ep_dbg_init();

	fprintf(EpStStddbg, "%s", EpVid->vidfgyellow);
	va_start(av, fmt);
	vfprintf(EpStStddbg, fmt, av);
	va_end(av);
	fprintf(EpStStddbg, "%s", EpVid->vidnorm);
}


/*
**  EP_DBG_SETSTREAM -- set output stream for debugging
**
**	Parameters:
**		fp -- the stream to use for debugging
**
**	Returns:
**		none
*/

void
ep_dbg_setstream(
	FILE *fp)
{
	if (fp != NULL && fp == EpStStddbg)
		return;

	// close the old stream
//	if (EpStStddbg != NULL)
//		(void) fclose(EpStStddbg);

	// if fp is NULL, switch to the default
	if (fp == NULL)
		EpStStddbg = stderr;
	else
		EpStStddbg = fp;
}

EP_STAT
ep_cvt_txt_to_debug(
	const char *vp,
	void *rp)
{
	ep_dbg_set(vp);

	return EP_STAT_OK;
}
