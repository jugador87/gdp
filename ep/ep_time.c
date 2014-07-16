#include <ep.h>
#include <ep_time.h>
#include <errno.h>

/*
**  EP_TIME_NOW --- return current time of day
**
**  	Ideally this will have resolution as good as a nanosecond,
**  	but it can be much less on most hardware.
*/

EP_STAT
ep_time_now(ep_timespec_t *tv)
{
	EP_STAT estat = EP_STAT_OK;

#ifdef CLOCK_REALTIME
	if (clock_gettime(CLOCK_REALTIME, tv) < 0)
	{
		estat = ep_stat_from_errno(errno);
		tv->tv_sec = EP_TIME_NOTIME;
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
