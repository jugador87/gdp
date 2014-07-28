/* vim: set ai sw=8 sts=8 ts=8 :*/

#include <ep.h>
#include <ep_time.h>
#include <errno.h>

/*
**  EP_TIME_NOW --- return current time of day
**
**  	Ideally this will have resolution as good as a nanosecond,
**  	but it can be much less on most hardware.
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

	return estat;
}


#define ONESECOND	1000000000LL

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
