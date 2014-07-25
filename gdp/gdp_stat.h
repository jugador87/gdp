/* vim: set ai sw=4 sts=4 ts=4: */

/***********************************************************************
 **
 **  GDP_STAT.H --- status codes specific to the Global Data Plane
 **
 **	If you add codes here, be sure to add a string description
 **	in gdp_stat.c.
 **
 ***********************************************************************/

#include <ep/ep_stat.h>
#include <ep/ep_registry.h>

// XXX	should really be in an include shared with other projects
//	to avoid conflicts in the future
#define GDP_MODULE	    1

#define GDP_STAT_NEW(sev, det)	    EP_STAT_NEW(EP_STAT_SEV_ ## sev,	\
					EP_REGISTRY_UCB, GDP_MODULE, det)

#define GDP_STAT_MSGFMT		    	GDP_STAT_NEW(ERROR, 1)
#define GDP_STAT_SHORTMSG	    	GDP_STAT_NEW(ERROR, 2)
#define GDP_STAT_READ_OVERFLOW	    	GDP_STAT_NEW(WARN, 3)
#define GDP_STAT_NOT_IMPLEMENTED    	GDP_STAT_NEW(SEVERE, 4)
#define GDP_STAT_PKT_WRITE_FAIL	    	GDP_STAT_NEW(ERROR, 5)
#define GDP_STAT_PKT_READ_FAIL	    	GDP_STAT_NEW(ERROR, 6)
#define GDP_STAT_PKT_VERSION_MISMATCH	GDP_STAT_NEW(SEVERE, 7)
#define GDP_STAT_PKT_NO_SEQ	    	GDP_STAT_NEW(ERROR, 8)
#define GDP_STAT_KEEP_READING		GDP_STAT_NEW(WARN, 9)
#define GDP_STAT_NOT_OPEN		GDP_STAT_NEW(ERROR, 10)
#define GDP_STAT_UNKNOWN_RID		GDP_STAT_NEW(SEVERE, 11)
#define GDP_STAT_INTERNAL_ERROR		GDP_STAT_NEW(ABORT, 12)
#define GDP_STAT_BAD_IOMODE		GDP_STAT_NEW(ERROR, 13)

// create EP_STAT from GDP protocol command codes for acks and naks
//	values from 128-254 reserved for this use
#define GDP_STAT_FROM_ACK(c)		GDP_STAT_NEW(OK, c)
#define GDP_STAT_FROM_NAK(c)		GDP_STAT_NEW(ERROR, c)
