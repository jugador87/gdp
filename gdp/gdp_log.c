/* vim: set ai sw=4 sts=4 ts=4 : */

/***********************************************************************
**		Copyright (c) 2008-2014, Eric P. Allman.  All rights reserved.
**		$Id: unix_syslog.c 288 2014-05-11 04:49:26Z eric $
***********************************************************************/

/*
**	Message logging for GDP.
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_stat.h>
#include <ep/ep_string.h>
#include <ep/ep_time.h>
#include <stdio.h>
#include <syslog.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>

static const char		*LogTag = NULL;
static int				LogFac = LOG_LOCAL4;
static FILE				*LogFile1 = NULL;
static FILE				*LogFile2 = NULL;
static bool				LogInitialized = false;

void
gdp_log_set(const char *tag,	// NULL => use program name
		int logfac,				// -1 => don't use syslog
		FILE *logfile,			// NULL => don't print to open file
		const char *fname)		// NULL => don't log to disk file
{
		LogTag = tag;
		LogFac = logfac;
		LogFile1 = logfile;
		if (fname == NULL)
				LogFile2 = NULL;
		else
				LogFile2 = fopen(fname, "a");
		LogInitialized = true;
}


static void
gdp_log_file(EP_STAT estat,
		char *fmt,
		va_list ap,
		EP_TIME_SPEC *tv,
		FILE *fp)
{
		char tbuf[40];
		char ebuf[100];
		struct tm *tm;
		time_t tvsec;

		ep_stat_tostr(estat, ebuf, sizeof ebuf);
		tvsec = tv->tv_sec;				//XXX may overflow if time_t is 32 bits!
		if ((tm = localtime(&tvsec)) == NULL)
				snprintf(tbuf, sizeof tbuf, "%"PRIu64".%06lu",
								tv->tv_sec, tv->tv_nsec / 1000L);
		else
		{
				char lbuf[40];

				snprintf(lbuf, sizeof lbuf, "%%Y-%%m-%%d %%H:%%M:%%S.%06lu %%z",
								tv->tv_nsec / 1000L);
				strftime(tbuf, sizeof tbuf, lbuf, tm);
		}

		fprintf(fp, "%s %s: ", tbuf, LogTag);
		vfprintf(fp, fmt, ap);
		fprintf(fp, "\n");
}


static void
gdp_log_syslog(EP_STAT estat, char *fmt, va_list ap)
{
		char ebuf[100];
		char mbuf[500];
		int sev = EP_STAT_SEVERITY(estat);
		int logsev;
		static bool inited = false;

		// initialize log if necessary
		if (!inited)
		{
				openlog(LogTag, LOG_PID, LogFac);
				inited = true;
		}

		// map estat severity to syslog priority
		switch (sev)
		{
		  case EP_STAT_SEV_OK:
				logsev = LOG_INFO;
				break;

		  case EP_STAT_SEV_WARN:
				logsev = LOG_WARNING;
				break;

		  case EP_STAT_SEV_ERROR:
				logsev = LOG_ERR;
				break;

		  case EP_STAT_SEV_SEVERE:
				logsev = LOG_CRIT;
				break;

		  case EP_STAT_SEV_ABORT:
				logsev = LOG_ALERT;
				break;

		  default:
				// %%% for lack of anything better
				logsev = LOG_ERR;
				break;

		}

		if (EP_STAT_ISOK(estat))
			strlcpy(ebuf, "OK", sizeof ebuf);
		else
			ep_stat_tostr(estat, ebuf, sizeof ebuf);
		vsnprintf(mbuf, sizeof mbuf, fmt, ap);
		syslog(logsev, "%s: %s", ebuf, mbuf);
		fprintf(stderr, "%s: %s\n", ebuf, mbuf);
}


void
gdp_log(EP_STAT estat, char *fmt, ...)
{
		va_list ap;
		EP_TIME_SPEC tv;

		ep_time_now(&tv);

		if (!LogInitialized)
		{
				LogFile1 = stderr;
				LogInitialized = true;
		}
		if (LogTag == NULL)
				LogTag = ep_app_getprogname();

		if (LogFac >= 0)
		{
				va_start(ap, fmt);
				gdp_log_syslog(estat, fmt, ap);
				va_end(ap);
		}
		if (LogFile1 != NULL)
		{
				fprintf(LogFile1, "%s", EpVid->vidfgcyan);
				va_start(ap, fmt);
				gdp_log_file(estat, fmt, ap, &tv, LogFile1);
				va_end(ap);
				fprintf(LogFile1, "%s", EpVid->vidnorm);
		}
		if (LogFile2 != NULL)
		{
				va_start(ap, fmt);
				gdp_log_file(estat, fmt, ap, &tv, LogFile2);
				va_end(ap);
		}
}
