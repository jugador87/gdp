/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP.H --- public headers for use of the Swarm Global Data Plane
*/

#ifndef _GDP_H_
#define _GDP_H_

#include <ep/ep.h>
#include <ep/ep_stat.h>
#include <ep/ep_time.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/time.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <gdp/gdp_buf.h>
#include <gdp/gdp_stat.h>

/**********************************************************************
**	Opaque structures
*/

// an open handle on a GCL (opaque)
typedef struct gdp_gcl		gdp_gcl_t;

/**********************************************************************
**	Other data types
*/

// the internal name of a GCL
typedef uint8_t				gcl_name_t[32];

// the printable name of a GCL
#define GDP_GCL_PNAME_LEN	43			// length of an encoded pname
typedef char				gcl_pname_t[GDP_GCL_PNAME_LEN + 1];

// a GDP request id (used for correlating commands and responses)
typedef uint32_t			gdp_rid_t;
#define PRIgdp_rid			PRIu32

// a GCL record number
typedef int64_t				gdp_recno_t;
#define PRIgdp_recno		PRId64

/*
**	I/O modes
**
**		A GCL can only be open for read or write (that is, append)
**		in any given instance.	There are no r/w instances.	 The other
**		modes here are for caching; ANY means to return NULL if
**		no GCLs are in the cache for a given connection.
*/

typedef enum
{
	GDP_MODE_ANY = 0,	// no mode specified
	GDP_MODE_RO = 1,	// read only
	GDP_MODE_AO = 2,	// append only
} gdp_iomode_t;


/**********************************************************************
**  Non-Opaque data types
*/

/*
**	 Datums
**		These are the underlying data unit that is passed through a GCL.
**
**		XXX Is the timestamp the commit timestamp into the dataplane
**			or the timestamp of the sample itself (if known)?  Do we
**			need two timestamps?  The sample timestamp is really an
**			application level concept, so arguably doesn't belong here.
**			But that's true of the location as well.
*/

typedef struct gdp_datum
{
	EP_THR_MUTEX		mutex;		// locking mutex (mostly for dbuf)
	struct gdp_datum	*next;		// next in free list
	bool				inuse:1;	// indicates that the datum is in use
	gdp_recno_t			recno;		// the record number
	EP_TIME_SPEC		ts;			// timestamp for this message
	size_t				dlen;		// length of data buffer (redundant)
	gdp_buf_t			*dbuf;		// data buffer
} gdp_datum_t;


/*
**	Events
**		gdp_event_t encodes an event.  Every event has a type and may
**		optionally have a GCL handle and/or a message.  For example,
**		data (from a subscription) has all three.
*/

typedef struct gdp_event	gdp_event_t;

// event types
#define GDP_EVENT_DATA		1	// returned data

// get next event (fills in gev structure)
extern gdp_event_t		*gdp_event_next(bool wait);

// free an event (required after gdp_event_next)
extern EP_STAT			gdp_event_free(gdp_event_t *gev);

// get the type of an event
extern int				gdp_event_gettype(gdp_event_t *gev);

// get the GCL handle
extern gdp_gcl_t		*gdp_event_getgcl(gdp_event_t *gev);

// get the datum
extern gdp_datum_t		*gdp_event_getdatum(gdp_event_t *gev);

/**********************************************************************
**	Public globals and functions
*/

struct event_base		*GdpIoEventBase;	// the base for GDP I/O events

// initialize the library
extern EP_STAT	gdp_init(void);

// run event loop (normally run from gdp_init; never returns)
extern void		*gdp_run_accept_event_loop(
					void *);				// unused

// create a new GCL
extern EP_STAT	gdp_gcl_create(
					gcl_name_t,
					gdp_gcl_t **);			// pointer to result GCL handle

// open an existing GCL
extern EP_STAT	gdp_gcl_open(
					gcl_name_t name,		// GCL name to open
					gdp_iomode_t rw,		// read/write (append)
					gdp_gcl_t **gclh);		// pointer to result GCL handle

// close an open GCL
extern EP_STAT	gdp_gcl_close(
					gdp_gcl_t *gclh);		// GCL handle to close

// append to a writable GCL
extern EP_STAT	gdp_gcl_publish(
					gdp_gcl_t *gclh,		// writable GCL handle
					gdp_datum_t *);			// message to write

// read from a readable GCL
extern EP_STAT	gdp_gcl_read(
					gdp_gcl_t *gclh,		// readable GCL handle
					gdp_recno_t recno,		// GCL record number
					gdp_datum_t *datum);	// pointer to result message

// subscribe to a readable GCL
//	If you don't specific cbfunc, events are generated instead
typedef void	(*gdp_gcl_sub_cbfunc_t)(  // the callback function
					gdp_gcl_t *gclh,		// the GCL triggering the call
					gdp_datum_t *datum,		// the message triggering the call
					void *cbarg);			// an arbitrary argument

extern EP_STAT	gdp_gcl_subscribe(
					gdp_gcl_t *gclh,		// readable GCL handle
					gdp_recno_t start,		// first record to retrieve
					gdp_recno_t stop,		// last record to retrieve
					gdp_gcl_sub_cbfunc_t cbfunc,
											// callback function for next datum
					void *cbarg);			// argument passed to callback

// return the name of a GCL
//		XXX: should this be in a more generic "getstat" function?
extern const gcl_name_t *gdp_gcl_getname(
					const gdp_gcl_t *gclh);		// open GCL handle

// check to see if a GCL name is valid
extern bool		gdp_gcl_name_is_zero(const gcl_name_t);

// print a GCL (for debugging)
extern void		gdp_gcl_print(
					const gdp_gcl_t *gclh,	// GCL handle to print
					FILE *fp,				// file to print it to
					int detail,				// not used at this time
					int indent);			// not used at this time

// make a printable GCL name from a binary version
char			*gdp_gcl_printable_name(
					const gcl_name_t internal,
					gcl_pname_t external);

// make a binary GCL name from a printable version
EP_STAT			gdp_gcl_internal_name(
					const gcl_pname_t external,
					gcl_name_t internal);

// parse a (possibly human-friendly) GCL name
EP_STAT			gdp_gcl_parse_name(
					const char *ext,
					gcl_name_t internal);

// allocate a new message
gdp_datum_t		*gdp_datum_new(void);

// free a message
void			gdp_datum_free(gdp_datum_t *);

// print a message (for debugging)
extern void		gdp_datum_print(
					const gdp_datum_t *datum,	// message to print
					FILE *fp);					// file to print it to

// get the record number from a datum
extern gdp_recno_t	gdp_datum_getrecno(
					const gdp_datum_t *datum);

// set a record number in a datum
extern void		gdp_datum_setrecno(
					gdp_datum_t *datum,
					gdp_recno_t recno);

// get the timestamp from a datum
extern void		gdp_datum_getts(
					const gdp_datum_t *datum,
					EP_TIME_SPEC *ts);

// set the timestamp in a datum
extern void		gdp_datum_setts(
					gdp_datum_t *datum,
					EP_TIME_SPEC *ts);

// get the data length from a datum
extern size_t	gdp_datum_getdlen(
					const gdp_datum_t *datum);

// get the data buffer from a datum
extern gdp_buf_t *gdp_datum_getdbuf(
					const gdp_datum_t *datum);


#endif // _GDP_H_
