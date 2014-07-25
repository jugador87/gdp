/*
 **  Definitions for event logging.
 */

#ifndef _GDP_LOG_H_
# define _GDP_LOG_H_

# include <ep/ep.h>
# include <sys/cdefs.h>

extern void EP_TYPE_PRINTFLIKE(2, 3)
gdp_log(EP_STAT estat, char *fmt, ...);

#endif // _GDP_LOG_H_
