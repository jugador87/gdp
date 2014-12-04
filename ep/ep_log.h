/* vim: set ai sw=8 sts=8 ts=8 : */

/*
**  Definitions for event logging.
*/

#ifndef _EP_LOG_H_
# define _EP_LOG_H_

# include <ep/ep.h>

extern void EP_TYPE_PRINTFLIKE(2, 3)
		ep_log(EP_STAT estat, char *fmt, ...);

#endif // _EP_LOG_H_
