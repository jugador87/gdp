/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_dbg.c 287 2014-04-29 18:18:23Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_app.h>
#include <ep_dbg.h>
#include <ep_string.h>
#include <ep_assert.h>
#include <fnmatch.h>
#include <pthread.h>

EP_SRC_ID("@(#)$Id: ep_dbg.c 287 2014-04-29 18:18:23Z eric $");

typedef struct FLAGPAT	FLAGPAT;

struct FLAGPAT
{
	const char	*pat;		// flag pattern (text)
	int		lev;		// level to set the pattern
	FLAGPAT		*next;		// next in chain
};

/*
**  This intentionally uses pthread primitives instead of ep_thr_*
**  to avoid recursive locks when calling ep_dbg_test.
*/

#if EP_OSCF_USE_PTHREADS
extern bool	_EpThrUsePthreads;
static pthread_rwlock_t	FlagListRwlock	= PTHREAD_RWLOCK_INITIALIZER;
#endif
static FLAGPAT	*FlagList;
FILE		*DebugFile;
int		__EpDbgCurGen;		// current generation number


/*
**  EP_DBG_INIT -- initialize debug subsystem
*/

void
ep_dbg_init(void)
{
	if (DebugFile == NULL)
		ep_dbg_setfile(NULL);
	EP_ASSERT(DebugFile != NULL);
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

	ep_dbg_init();

	if (f == NULL)
		return;
	while (*f != '\0')
	{
		int i = 0;
		char pbuf[200];

		if (strchr(",; ", *f) != NULL)
		{
			f++;
			continue;
		}

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
		f += strcspn(f, ",;");
	}
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
#if EP_OSCF_USE_PTHREADS
	if (_EpThrUsePthreads)
		pthread_rwlock_wrlock(&FlagListRwlock);
#endif
	fp->next = FlagList;
	FlagList = fp;
	__EpDbgCurGen++;
#if EP_OSCF_USE_PTHREADS
	if (_EpThrUsePthreads)
		pthread_rwlock_unlock(&FlagListRwlock);
#endif
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

#if EP_OSCF_USE_PTHREADS
	if (_EpThrUsePthreads)
		pthread_rwlock_rdlock(&FlagListRwlock);
#endif
	flag->gen = __EpDbgCurGen;
	for (fp = FlagList; fp != NULL; fp = fp->next)
	{
		if (fnmatch(fp->pat, flag->name, 0) == 0)
			break;
	}
	if (fp == NULL)
		flag->level = 0;
	else
		flag->level = fp->lev;
#if EP_OSCF_USE_PTHREADS
	if (_EpThrUsePthreads)
		pthread_rwlock_unlock(&FlagListRwlock);
#endif

	return flag->level;
}


#if 0
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
	if (DebugFile == NULL)
		ep_dbg_init();

	if (ep_dbg_test(flag, level))
	{
		va_list av;

		fprintf(DebugFile, "%s", EpVid->vidfgyellow);
		va_start(av, fmt);
		vfprintf(DebugFile, fmt, av);
		va_end(av);
		fprintf(DebugFile, "%s", EpVid->vidnorm);
		return true;
	}
	return false;
}
#endif


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

	if (DebugFile == NULL)
		ep_dbg_init();

	flockfile(DebugFile);
	fprintf(DebugFile, "%s", EpVid->vidfgyellow);
	va_start(av, fmt);
	vfprintf(DebugFile, fmt, av);
	va_end(av);
	fprintf(DebugFile, "%s", EpVid->vidnorm);
	funlockfile(DebugFile);
}


/*
**  EP_DBG_SETFILE -- set output file for debugging
**
**	Parameters:
**		fp -- the file to use for debugging; if NULL it
**			uses the default (stderr).
**
**	Returns:
**		none
*/

void
ep_dbg_setfile(
	FILE *fp)
{
	if (fp != NULL && fp == DebugFile)
		return;

	// close the old stream
	if (DebugFile != NULL && DebugFile != stdout && DebugFile != stderr)
		(void) fclose(DebugFile);

	// if fp is NULL, switch to the default
	if (fp != NULL)
	{
		DebugFile = fp;
	}
	else
	{
		const char *dfile;

		dfile = ep_adm_getstrparam("libep.dbg.file", "stderr");
		if (strcmp(dfile, "stderr") == 0)
			DebugFile = stderr;
		else if (strcmp(dfile, "stdout") == 0)
			DebugFile = stdout;
		else
		{
			DebugFile = fopen(dfile, "a");
			if (DebugFile == NULL)
			{
				ep_app_warn("Cannot open debug file %s",
						dfile);
				DebugFile = stderr;
			}
		}
		setlinebuf(DebugFile);
	}
}


FILE *
ep_dbg_getfile(void)
{
	if (DebugFile == NULL)
		ep_dbg_init();
	return DebugFile;
}


EP_STAT
ep_cvt_txt_to_debug(
	const char *vp,
	void *rp)
{
	ep_dbg_set(vp);

	return EP_STAT_OK;
}
