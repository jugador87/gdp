/* vim: set ai sw=4 sts=4 ts=4: */

/**********************************************************************
 **  Time intervals.  Shamelessly stolen from TrueTime.
 **
 **	See "Spanner: Google's Globally-Distributed Database",
 **	    Proceedings of OSDI 2012.
 **	    (https://www.usenix.org/system/files/conference/osdi12/osdi12-final-16.pdf)
 **	Briefly, this abstraction explicitly acknowledges clock skew.  The
 **	    concept of "now" is actually a range indicating a confidence
 **	    interval.
 */

#ifndef _GDP_TIMESTAMP_H_
#define _GDP_TIMESTAMP_H_   1

#include <time.h>
#include <sys/time.h>

// a timestamp --- a single instant in time
//	timespec has nanosecond resolution but may not be as portable as timeval
typedef struct timespec tt_stamp_t;

// a time interval
typedef struct
{
	tt_stamp_t stamp;		// center of interval
	uint32_t accuracy;	// clock accuracy in nanoseconds
} tt_interval_t;

// a sentinel value for stamp.tv_sec to indicate invalidity
//  XXX zero is a horrid value, since it indicates a valid time.  Should
//	be something like INT32_MIN or INT64_MIN depending on the size
//	of the tv_sec field.
#define TT_NOTIME		0

extern EP_STAT
tt_now(tt_interval_t *t);	// return current time
extern bool
tt_before(const tt_stamp_t t);	// true if t has passed
extern bool
tt_after(const tt_stamp_t t);	// true if t has not started
extern void
tt_print_stamp(const tt_stamp_t *t, FILE *fp);
extern EP_STAT
tt_parse_stamp(const char *, tt_stamp_t *);
extern void
tt_print_interval(const tt_interval_t *t, FILE *fp, bool human);
extern EP_STAT
tt_parse_interval(const char *, tt_interval_t *);
extern void
tt_set_clock_accuracy(long nsec);

#endif // _GDP_TIMESTAMP_H_
