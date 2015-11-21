/* vim: set ai sw=8 sts=8 ts=8 : */

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
**  Message logging.
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_stat.h>
#include <ep/ep_string.h>
#include <ep/ep_syslog.h>
#include <ep/ep_time.h>

#include <inttypes.h>
#include <time.h>
#include <sys/cdefs.h>
#include <sys/time.h>

static const char	*LogTag = NULL;
static int		LogFac = -1;
static FILE		*LogFile1 = NULL;
static FILE		*LogFile2 = NULL;
static bool		LogInitialized = false;

void
ep_log_init(const char *tag,	// NULL => use program name
	int logfac,		// -1 => don't use syslog
	FILE *logfile,		// NULL => don't print to open file
	const char *fname)	// NULL => don't log to disk file
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
ep_log_file(EP_STAT estat,
	const char *fmt,
	va_list ap,
	EP_TIME_SPEC *tv,
	FILE *fp)
{
	char tbuf[40];
	struct tm *tm;
	time_t tvsec;

	tvsec = tv->tv_sec;		//XXX may overflow if time_t is 32 bits!
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
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_stat_tostr(estat, ebuf, sizeof ebuf);
		fprintf(fp, ": %s", ebuf);
	}
	fprintf(fp, "\n");
}


static void
ep_log_syslog(EP_STAT estat, const char *fmt, va_list ap)
{
	char ebuf[100];
	char mbuf[500];
	int sev = EP_STAT_SEVERITY(estat);
	int logsev;
	static bool inited = false;
	const char *logline = "%s: %s";

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
		logline = "%s";
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
	syslog(logsev, logline, ebuf, mbuf);
}


void
ep_log(EP_STAT estat, const char *fmt, ...)
{
	va_list ap;
	EP_TIME_SPEC tv;

	ep_time_now(&tv);

	if (!LogInitialized)
	{
		const char *facname;
		int logfac;

		facname = ep_adm_getstrparam("libep.log.facility", NULL);
		if (facname == NULL)
			logfac = -1;
		else
			logfac = ep_syslog_fac_from_name(facname);

		ep_log_init(ep_app_getprogname(), logfac, stderr, NULL);
	}

	if (LogFac >= 0)
	{
		va_start(ap, fmt);
		ep_log_syslog(estat, fmt, ap);
		va_end(ap);
	}
	if (LogFile1 != NULL)
	{
		fprintf(LogFile1, "%s", EpVid->vidfgcyan);
		va_start(ap, fmt);
		ep_log_file(estat, fmt, ap, &tv, LogFile1);
		va_end(ap);
		fprintf(LogFile1, "%s\n", EpVid->vidnorm);
	}
	if (LogFile2 != NULL)
	{
		va_start(ap, fmt);
		ep_log_file(estat, fmt, ap, &tv, LogFile2);
		va_end(ap);
	}
}
