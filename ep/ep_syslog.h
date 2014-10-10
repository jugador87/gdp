/* vim: set ai sw=8 sts=8 ts=8 : */

#ifndef _EP_SYSLOG_H_
#define _EP_SYSLOG_H_	1

#include <syslog.h>

extern int	ep_syslog_pri_from_name(	// get priority by name
			const char *name);
extern char	*ep_syslog_name_from_pri(	// get name by priority
			int pri);
extern int	ep_syslog_fac_from_name(	// get facility by name
			const char *name);
extern char	*ep_syslog_name_from_fac(	// get name by facility
			int fac);

#endif // _EP_SYSLOG_H_
