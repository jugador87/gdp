#ifndef _EP_TIME_H_
#define _EP_TIME_H_

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

typedef struct timespec		EP_TIME_SPEC;

extern EP_STAT	ep_time_now(EP_TIME_SPEC *tv);

#define EP_TIME_NOTIME		(INT64_MIN)

#endif //_EP_TIME_H_
