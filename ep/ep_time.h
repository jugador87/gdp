/* vim: set ai sw=8 sts=8 ts=8 :*/

#ifndef _EP_TIME_H_
#define _EP_TIME_H_

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct
{
	int64_t		tv_sec;		// seconds since Jan 1, 1970
	uint32_t	tv_nsec;	// nanoseconds
	float		tv_accuracy;	// clock accuracy in seconds
} EP_TIME_SPEC;
#pragma pack(pop)

#define EP_TIME_NOTIME		(INT64_MIN)

// return current time
extern EP_STAT	ep_time_now(EP_TIME_SPEC *tv);

// return current time plus an offset
extern EP_STAT	ep_time_deltanow(uint64_t delta, EP_TIME_SPEC *tv);

// return putative clock accuracy
extern float	ep_time_accuracy(void);

// set the clock accuracy (may not be available)
extern void	ep_time_setaccuracy(float acc);

// format a time string into a buffer
extern void	ep_time_format(const EP_TIME_SPEC *tv,
				char *buf,
				size_t bz,
				bool human);

// format a time string to a file
extern void	ep_time_print(const EP_TIME_SPEC *tv,
				FILE *fp,
				bool human);

// parse a time string
extern EP_STAT	ep_time_parse(const char *timestr,
				EP_TIME_SPEC *tv);

// sleep for the indicated number of nanoseconds
extern EP_STAT	ep_time_nanosleep(int64_t);

// test to see if a timestamp is valid
#define EP_TIME_ISVALID(ts)	((ts)->tv_sec != EP_TIME_NOTIME)

// invalidate a timestamp
#define EP_TIME_INVALIDATE(ts)	((ts)->tv_sec = EP_TIME_NOTIME)

#endif //_EP_TIME_H_
