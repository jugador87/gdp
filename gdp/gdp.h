/* vim: set ai sw=4 sts=4 :*/

/*
 **  GDP.H --- public headers for use of the Swarm Global Data Plane
 */

#ifndef _GDP_H_
#define _GDP_H_

#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ep/ep_stat.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <gdp/gdp_timestamp.h>
#include <gdp/gdp_stat.h>

/**********************************************************************
 **  Opaque structures
 */

// XXX not clear if we need this publicly exposed (or even what's in it)
typedef struct gcl_t gcl_t;

// an open handle on a GCL (opaque)
typedef struct gcl_handle_t gcl_handle_t;

// the internal name of a GCL
typedef uint8_t gcl_name_t[32];

// the printable name of a GCL
#define GCL_PNAME_LEN	    43		// length of an encoded pname
typedef char gcl_pname_t[GCL_PNAME_LEN + 1];

// a request id (used for correlating commands and responses)
typedef uint64_t gdp_rid_t;

// a message number
typedef int32_t gdp_msgno_t;

/*
 **  I/O modes
 **
 **	A GCL can only be open for read or write (that is, append)
 **	in any given instance.  There are no r/w instances.  The other
 **	modes here are for caching; ANY means to return NULL if
 **	no GCLs are in the cache for a given connection.
 */

typedef enum
{
	GDP_MODE_ANY = 0,	// no mode specified
	GDP_MODE_RO = 1,	// read only
	GDP_MODE_AO = 2,	// append only
} gdp_iomode_t;

/**********************************************************************
 **   Messages
 **	These are the underlying unit that is passed through a GCL.
 **
 **	XXX Is the timestamp the commit timestamp into the dataplane
 **	    or the timestamp of the sample itself (if known)?  Do we
 **	    need two timestamps?  The sample timestamp is really an
 **	    application level concept, so arguably doesn't belong here.
 **	    But that's true of the location as well.
 */

// this is a horrid hack for use only by gdpd
#ifndef GDP_MSG_EXTRA
# define GDP_MSG_EXTRA
#endif

typedef struct
{
	tt_interval_t ts;		    // timestamp for this message
	gdp_msgno_t msgno;	    // the message number
	void *data;	    // pointer to data
	size_t len;	    // length of data
GDP_MSG_EXTRA// used by gdpd
} gdp_msg_t;

/**********************************************************************
 **  Public globals and functions
 */

struct event_base *GdpEventBase;	// the base for all GDP events

// initialize the library
extern EP_STAT
gdp_init(
bool run_event_loop);	// run event loop in thread

// run event loop (normally run from gdp_init; never returns)
extern void *
gdp_run_accept_event_loop(void *);		// unused

// create a new GCL
extern EP_STAT
gdp_gcl_create(gcl_t *,		// type information (unused)
    gcl_name_t, gcl_handle_t **);	// pointer to result GCL handle

// open an existing GCL
extern EP_STAT
gdp_gcl_open(gcl_name_t name,	// GCL name to open
    gdp_iomode_t rw,	// read/write (append)
    gcl_handle_t **gclh);	// pointer to result GCL handle

// close an open GCL
extern EP_STAT
gdp_gcl_close(gcl_handle_t *gclh);	// GCL handle to close

// create a new GCL message
extern EP_STAT
gdp_gcl_msg_new(gcl_handle_t *gclh, gdp_msg_t **);	// result area for message

// append to a writable GCL
extern EP_STAT
gdp_gcl_append(gcl_handle_t *gclh,	// writable GCL handle
    gdp_msg_t *);		// message to write

// read from a readable GCL
extern EP_STAT
gdp_gcl_read(gcl_handle_t *gclh,	// readable GCL handle
    long msgno,		// offset into GCL (msg number)
    gdp_msg_t *msg,		// pointer to result message
    struct evbuffer *revb);	// reply buffer to receive data

// subscribe to a readable GCL
typedef void
(*gdp_gcl_sub_cbfunc_t)(  // the callback function
    gcl_handle_t *gclh,	// the GCL handle triggering the call
    gdp_msg_t *msg,		// the message triggering the call
    void *cbarg);		// an arbitrary argument

extern EP_STAT
gdp_gcl_subscribe(gcl_handle_t *gclh,	// readable GCL handle
    gdp_gcl_sub_cbfunc_t cbfunc,
    // callback function for next msg
    long off,		// starting offset
    void *buf,		// buffer space for msg
    size_t buflen,		// length of buf
    void *cbarg);		// argument passed to callback

// return the name of a GCL
//	XXX: should this be in a more generic "getstat" function?
extern const gcl_name_t *
gdp_gcl_getname(const gcl_handle_t *gclh);	// open GCL handle

// check to see if a GCL name is valid
extern bool
gdp_gcl_name_is_zero(const gcl_name_t);

// print a GCL (for debugging)
extern EP_STAT
gdp_gcl_print(const gcl_handle_t *gclh,	// GCL handle to print
    FILE *fp,		// file to print it to
    int detail,		// not used at this time
    int indent);		// not used at this time

// print a GCL message (for debugging)
extern void
gdp_gcl_msg_print(const gdp_msg_t *msg,	// message to print
    FILE *fp);		// file to print it to

// make a printable GCL name from a binary version
void
gdp_gcl_printable_name(const gcl_name_t internal, gcl_pname_t external);

// make a binary GCL name from a printable version
EP_STAT
gdp_gcl_internal_name(const gcl_pname_t external, gcl_name_t internal);

#endif // _GDP_H_
