/* vim: set ai sw=8 sts=8 ts=8 : */

/*
**  Definitions for event logging.
*/

#ifndef _EP_LOG_H_
# define _EP_LOG_H_

# include <ep/ep.h>

extern void	ep_log_init(
			const char *tag,	// NULL => use program name
			int logfac,		// -1   => don't use syslog
			FILE *logfile,		// NULL => don't log to open file
			const char *fname);	// NULL => don't log to disk file

extern void EP_TYPE_PRINTFLIKE(2, 3)
		ep_log(EP_STAT estat, const char *fmt, ...);

#endif // _EP_LOG_H_
