/* vim: set ai sw=8 sts=8 ts=8 :*/

#include <ep.h>
#include <ep_app.h>
#include <ep_string.h>
#include <ep_time.h>
#include <errno.h>
#include <inttypes.h>


/*
**  EP_TIME_ACCURACY --- return the accuracy of the clock
**
**	Some systems may be able to determine this more precisely; we
**	just take it as an administrative parameter.
*/

static float	ClockAccuracy = -1;		// in seconds

#define ONESECOND	INT64_C(1000000000)	// one second in nanoseconds

float
ep_time_accuracy(void)
{
	if (ClockAccuracy < 0)
	{
		const char *p = ep_adm_getstrparam("libep.time.accuracy", NULL);

		if (p == NULL)
			ClockAccuracy = 0.0;
		else
			ClockAccuracy = strtof(p, NULL);
	}
	return ClockAccuracy;
}


/*
**  EP_TIME_NOW --- return current time of day
**
**	Ideally this will have resolution as good as a nanosecond,
**	but it can be much less on most hardware.
**
**	Everything gets copied because some platforms *still* use
**	32-bit time_t.  Beware of January 18, 2038!
*/

#undef CLOCK_REALTIME		// issues on Linux; punt for now

EP_STAT
ep_time_now(EP_TIME_SPEC *tv)
{
	EP_STAT estat = EP_STAT_OK;

#ifdef CLOCK_REALTIME
	struct timespec tvs;

	if (clock_gettime(CLOCK_REALTIME, &tvs) < 0)
	{
		estat = ep_stat_from_errno(errno);
		tv->tv_sec = EP_TIME_NOTIME;
	}
	else
	{
		tv->tv_sec = tvs.tv_sec;
		tv->tv_nsec = tvs.tv_nsec;
	}
#else
	struct timeval tvu;

	if (gettimeofday(&tvu, NULL) < 0)
	{
		estat = ep_stat_from_errno(errno);
		tv->tv_sec = EP_TIME_NOTIME;
	}
	else
	{
		tv->tv_sec = tvu.tv_sec;
		tv->tv_nsec = tvu.tv_usec * 1000;
	}
#endif

	tv->tv_accuracy = ep_time_accuracy();

	return estat;
}


/*
**  EP_TIME_DELTANOW --- return current time of day plus a delta
**
**	This assumes that a negative delta is represented as both
**	a non-positive number of seconds and a non-positive number
**	of nanoseconds; for example, { -5, +500000 } wouldn't make
**	sense.
*/

EP_STAT
ep_time_deltanow(EP_TIME_SPEC *delta, EP_TIME_SPEC *tv)
{
	EP_STAT estat;

	estat = ep_time_now(tv);
	EP_STAT_CHECK(estat, return estat);

	ep_time_add_delta(delta, tv);
	return EP_STAT_OK;
}


/*
**  EP_TIME_ADD --- add a delta to a time
*/

void
ep_time_add_delta(EP_TIME_SPEC *delta, EP_TIME_SPEC *tv)
{
	tv->tv_sec += delta->tv_sec;
	tv->tv_nsec += delta->tv_nsec;
	if (tv->tv_nsec > ONESECOND)
	{
		tv->tv_sec++;
		tv->tv_nsec -= ONESECOND;
	}
	else if (tv->tv_nsec < 0)
	{
		tv->tv_sec--;
		tv->tv_nsec += ONESECOND;
	}
}


/*
**  EP_TIME_BEFORE --- true if A occurred before B
**
**	This doesn't allow for clock precision; it should really have
**	a "maybe" return.
*/

bool
ep_time_before(EP_TIME_SPEC *a, EP_TIME_SPEC *b)
{
	if (a->tv_sec < b->tv_sec)
		return true;
	else if (a->tv_sec > b->tv_sec)
		return false;
	else
		return (a->tv_nsec < b->tv_nsec);
}


/*
**  EP_TIME_FROM_NANOSEC --- convert nanoseconds to a EP_TIME_SPEC
*/

void
ep_time_from_nsec(int64_t nsec, EP_TIME_SPEC *tv)
{
	tv->tv_sec = nsec / ONESECOND;
	tv->tv_nsec = nsec % ONESECOND;
}


/*
**  EP_TIME_NANOSLEEP --- sleep for designated number of nanoseconds
*/

EP_STAT
ep_time_nanosleep(int64_t nsec)
{
	struct timespec ts;

	ts.tv_sec = nsec / ONESECOND;
	ts.tv_nsec = nsec % ONESECOND;
	if (nanosleep(&ts, NULL) < 0)
		return ep_stat_from_errno(errno);
	return EP_STAT_OK;
}


/*
**  EP_TIME_FORMAT --- format a time/date for printing
*/

void
ep_time_format(const EP_TIME_SPEC *tv, char *tbuf, size_t tbsiz, bool human)
{
	if (!EP_TIME_ISVALID(tv))
	{
		snprintf(tbuf, tbsiz, "%s", human ? "(none)" : "-");
		return;
	}

	struct tm tm;
	time_t tvsec;
	char xbuf[40];
	char ybuf[40];

	tvsec = tv->tv_sec;	// may overflow if time_t is 32 bits!
	gmtime_r(&tvsec, &tm);
	strftime(xbuf, sizeof xbuf, "%Y-%m-%dT%H:%M:%S", &tm);
	if (tv->tv_accuracy != 0.0)
	{
		if (human)
		{
			snprintf(ybuf, sizeof ybuf, " %s %#f",
				EpChar->plusminus, tv->tv_accuracy);
		}
		else
		{
			snprintf(ybuf, sizeof ybuf, "/%f", tv->tv_accuracy);
		}
	}
	else
	{
		ybuf[0] = '\0';
	}
	snprintf(tbuf, tbsiz, "%s.%09" PRIu32 "Z%s", xbuf, tv->tv_nsec, ybuf);
}


/*
**  EP_TIME_PRINT --- print a time/date spec to a file
*/

void
ep_time_print(const EP_TIME_SPEC *tv, FILE *fp, bool human)
{
	char tbuf[100];

	ep_time_format(tv, tbuf, sizeof tbuf, human);
	fprintf(fp, "%s", tbuf);
}


/*
**  EP_TIME_PARSE --- convert external to internal format
**
**	Only works for a very specific format.
*/

EP_STAT
ep_time_parse(const char *timestr, EP_TIME_SPEC *tv)
{
	int i;
	struct tm tm;
	int nbytes;

	if ((i = sscanf(timestr, "%d-%d-%dT%d:%d:%d.%uldZ%n",
			&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
			&tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			&tv->tv_nsec, &nbytes)) < 7)
	{
		return EP_STAT_TIME_BADFORMAT;
	}

	tm.tm_year -= 1900;
	tm.tm_mon -=1;
	tv->tv_sec = timegm(&tm);

	timestr += nbytes;
	if (*timestr++ == '/')
	{
		int mbytes;

		sscanf(timestr, "%f%n", &tv->tv_accuracy, &mbytes);
		nbytes += mbytes + 1;
	}

	return EP_STAT_FROM_INT(nbytes);
}
