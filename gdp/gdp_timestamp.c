/* vim: set ai sw=4 sts=4 :*/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_stat.h>
#include <ep/ep_string.h>
#include <gdp/gdp_timestamp.h>
#include <errno.h>

static EP_DBG Dbg = EP_DBG_INIT("gdp.timestamp", "GDP Timestamping");

#define GDP_STAT_STAMPFMT   EP_STAT_ERROR

// if using a timeval instead of timespec, this will be 1000000 usec
#define ONESECOND	1000000000L	// in nanoseconds

long ClockAccuracy = -1;		// in nanoseconds

EP_STAT
tt_now(tt_interval_t *tt)
{
	EP_STAT estat = EP_STAT_OK;

#ifdef CLOCK_REALTIME
	if (clock_gettime(CLOCK_REALTIME, &tt->stamp) < 0)
	{
		estat = ep_stat_from_errno(errno);
		tt->stamp.tv_sec = tt->stamp.tv_nsec = TT_NOTIME;
	}
#else
	struct timeval tvu;

	if (gettimeofday(&tvu, NULL) < 0)
	{
		estat = ep_stat_from_errno(errno);
		tt->stamp.tv_sec = tt->stamp.tv_nsec = TT_NOTIME;
	}
	else
	{
		tt->stamp.tv_sec = tvu.tv_sec;
		tt->stamp.tv_nsec = tvu.tv_usec * 1000;
	}
#endif

	// determine possible clock drift
	if (ClockAccuracy < 0)
	{
		// get a default clock jitter
		// for now this is just an administrative parameter
		ClockAccuracy = ep_adm_getlongparam("swarm.gdp.clock.accuracy",
		ONESECOND);
		if (ClockAccuracy < 0)
			ClockAccuracy = ONESECOND;
	}
	tt->accuracy = ClockAccuracy;

	if (ep_dbg_test(Dbg, 19))
	{
		ep_dbg_printf("tt_now => ");
		tt_print_interval(tt, ep_dbg_getfile(), true);
		ep_dbg_printf("\n");
	}

	return estat;
}

void
tt_print_stamp(const tt_stamp_t *ts, FILE *fp)
{
	if (ts->tv_sec == TT_NOTIME)
	{
		fprintf(fp, "(none)");
	}
	else
	{
		struct tm tm;
		char tbuf[40];

		gmtime_r(&ts->tv_sec, &tm);
		strftime(tbuf, sizeof tbuf, "%Y-%m-%dT%H:%M:%S", &tm);
		fprintf(fp, "%s.%09ldZ", tbuf, ts->tv_nsec);
	}
}

EP_STAT
tt_parse_stamp(const char *timestr, tt_stamp_t *ts)
{
	struct tm tm;
	int nbytes;
	int i;

	ep_dbg_cprintf(Dbg, 20, "tt_parse_stamp: <<< %s\n", timestr);
	if ((i = sscanf(timestr, "%d-%d-%dT%d:%d:%d.%ldZ%n", &tm.tm_year,
	        &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
	        &ts->tv_nsec, &nbytes)) < 7)
	{
		// failure; couldn't read enough data
		ep_dbg_cprintf(Dbg, 1,
		        "tt_parse_stamp: >>> Bad format: '%.30s', only %d fields\n",
		        timestr, i);
		return GDP_STAT_STAMPFMT;
	}

	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	ts->tv_sec = timegm(&tm);
	if (ep_dbg_test(Dbg, 20))
	{
		ep_dbg_printf("tt_parse_stamp: >>> ");
		tt_print_stamp(ts, ep_dbg_getfile());
		ep_dbg_printf(", nbytes = %d\n", nbytes);
	}
	return EP_STAT_FROM_LONG(nbytes);	// OK status with detail
}

void
tt_print_interval(const tt_interval_t *ti, FILE *fp, bool human)
{
	if (ti->stamp.tv_sec == TT_NOTIME)
		fprintf(fp, human ? "(none)" : "-");
	else
	{
		tt_print_stamp(&ti->stamp, fp);
		if (human)
		{
			fprintf(fp, " %s %ld.%09ld", EpChar->plusminus,
			        ti->accuracy / ONESECOND, ti->accuracy % ONESECOND);
		}
		else
		{
			fprintf(fp, "/%u", ti->accuracy);
		}
	}
}

const char *
tt_format_interval(tt_interval_t *ts, char *tsbuf, size_t tsbufsiz, bool human)
{
	FILE *fp;

	fp = ep_fopensmem(tsbuf, tsbufsiz, "w");
	if (fp == NULL)
		return "XXX-cannot-format-interval-XXX";
	tt_print_interval(ts, fp, human);
	fputc('\0', fp);
	fclose(fp);
	// null terminate even if tsbuf overflows
	tsbuf[tsbufsiz - 1] = '\0';
	return tsbuf;
}

EP_STAT
tt_parse_interval(const char *timestr, tt_interval_t *ti)
{
	EP_STAT estat;
	long nbytes;
	char *endptr;

	estat = tt_parse_stamp(timestr, &ti->stamp);
	EP_STAT_CHECK(estat, return estat);

	nbytes = EP_STAT_TO_LONG(estat);
	timestr += nbytes;
	if (*timestr++ != '/')
	{
		ep_dbg_cprintf(Dbg, 1, "Bad format interval, '/' expected\n");
		return GDP_STAT_STAMPFMT;
	}
	ti->accuracy = strtol(timestr, &endptr, 10);

	return EP_STAT_FROM_LONG((endptr - timestr) + nbytes);
}

void
tt_set_clock_accuracy(long res)
{
	if (res >= 0)
		ClockAccuracy = res;
}
