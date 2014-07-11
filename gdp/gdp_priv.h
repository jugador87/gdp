/* vim: set ai sw=4 sts=4 : */

/*
**  These headers are not intended for external use.
*/

#ifndef _GDP_PRIV_H_
#define _GDP_PRIV_H_

#include <gdp/gdp_timestamp.h>
#include <pthread.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

#include <stdio.h>

struct gcl_handle_t
{
    // fields used by GDP library
    pthread_mutex_t	mutex;		// lock on this data structure
    pthread_cond_t	cond;		// pthread wakeup signal
    gcl_name_t		gcl_name;	// the internal name
    gdp_iomode_t	iomode;		// read only or append only

    // transient (should only be used when locked)
    long		msgno;		// msgno from last operation
    struct evbuffer	*revb;		// buffer for return results
    uint32_t		flags;		// see below
    EP_STAT		estat;		// status code from last operation
    tt_interval_t	ts;		// timestamp from last operation

    // fields used only by gdpd
    long		ver;		// version number of on-disk file
    FILE		*fp;		// pointer to the on-disk file
    off_t		*offcache;	// offsets of records we have seen
    long		cachesize;	// size of offcache array
    long		maxmsgno;	// last msgno that we have read/written
};

// GCL flags
#define GCLH_DONE	0x00000001	// operation complete
#define GCLH_ASYNC	0x00000002	// do async I/O on this GCL handle

#define GCL_NEXT_MSG	(-1)		// sentinel for next available message

gcl_handle_t	*gdp_gcl_cache_get(
				gcl_name_t gcl_name,
				gdp_iomode_t mode);

void		gdp_gcl_cache_add(
				gcl_handle_t *gclh,
				gdp_iomode_t mode);

void		gdp_gcl_cache_drop(
				gcl_name_t gcl_name,
				gdp_iomode_t mode);

EP_STAT		gdp_gcl_cache_init(void);

const char	*_gdp_proto_cmd_name(uint8_t cmd);

EP_STAT		_gdp_start_event_loop_thread(
				struct event_base *evb);

void		gdp_gcl_newname(
				gcl_name_t gcl_name);

#endif // _GDP_PRIV_H_
