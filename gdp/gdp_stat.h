/***********************************************************************
**
**  GDP_STAT.H --- status codes specific to the Global Data Plane
**
***********************************************************************/

#include <ep/ep_stat.h>
#include <ep/ep_registry.h>

// should really be in an include shared with other projects
//	to avoid conflicts in the future
#define GDP_MODULE	    1

#define GDP_STAT_NEW(sev, det)	    EP_STAT_NEW(EP_STAT_SEV_ ## sev,	\
					EP_REGISTRY_UCB, GDP_MODULE, det)

#define GDP_STAT_MSGFMT	    GDP_STAT_NEW(ERROR, 1)
#define GDP_STAT_SHORTMSG   GDP_STAT_NEW(ERROR, 2)
