/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP.H --- public headers for use of the Swarm Global Data Plane
*/

#ifndef _GDP_H_
#define _GDP_H_

#include <stdbool.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ep/ep_stat.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <gdp/gdp_buf.h>
#include <gdp/gdp_stat.h>
#include <gdp/gdp_timestamp.h>

/**********************************************************************
**	Opaque structures
*/

// an open handle on a GCL (opaque)
typedef struct gcl_handle	gcl_handle_t;

/**********************************************************************
**	Other data types
*/

// the internal name of a GCL
typedef uint8_t				gcl_name_t[32];

// the printable name of a GCL
#define GCL_PNAME_LEN		43			// length of an encoded pname
typedef char				gcl_pname_t[GCL_PNAME_LEN + 1];

// a GDP request id (used for correlating commands and responses)
typedef uint32_t			gdp_rid_t;

// a GCL record number
typedef int32_t				gdp_recno_t;

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
**	 Messages
**		These are the underlying unit that is passed through a GCL.
**
**		XXX Is the timestamp the commit timestamp into the dataplane
**			or the timestamp of the sample itself (if known)?  Do we
**			need two timestamps?  The sample timestamp is really an
**			application level concept, so arguably doesn't belong here.
**			But that's true of the location as well.
*/

typedef struct gdp_msg
{
	EP_THR_MUTEX		mutex;		// locking mutex (mostly for dbuf)
	LIST_ENTRY(gdp_msg)	list;		// linked list for free list management
	gdp_recno_t			recno;		// the record number
	tt_interval_t		ts;			// timestamp for this message
	size_t				dlen;		// length of data buffer (redundant)
	gdp_buf_t			*dbuf;		// data buffer
} gdp_msg_t;


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
					gcl_handle_t **);		// pointer to result GCL handle

// open an existing GCL
extern EP_STAT	gdp_gcl_open(
					gcl_name_t name,		// GCL name to open
					gdp_iomode_t rw,		// read/write (append)
					gcl_handle_t **gclh);	// pointer to result GCL handle

// close an open GCL
extern EP_STAT	gdp_gcl_close(
					gcl_handle_t *gclh);	// GCL handle to close

// create a new GCL message
extern EP_STAT	gdp_gcl_msg_new(
					gcl_handle_t *gclh,
					gdp_msg_t **);			// result area for message

// append to a writable GCL
extern EP_STAT	gdp_gcl_append(
					gcl_handle_t *gclh,		// writable GCL handle
					gdp_msg_t *);			// message to write

// read from a readable GCL
extern EP_STAT	gdp_gcl_read(
					gcl_handle_t *gclh,		// readable GCL handle
					gdp_recno_t recno,		// GCL record number
					gdp_buf_t *dbuf,		// reply buffer to receive data
					gdp_msg_t *msg);		// pointer to result message

#if 0
// subscribe to a readable GCL
typedef void	(*gdp_gcl_sub_cbfunc_t)(  // the callback function
					gcl_handle_t *gclh,		// the GCL handle triggering the call
					gdp_msg_t *msg,			// the message triggering the call
					void *cbarg);			// an arbitrary argument

extern EP_STAT	gdp_gcl_subscribe(
					gcl_handle_t *gclh,		// readable GCL handle
					gdp_gcl_sub_cbfunc_t cbfunc,
											// callback function for next msg
					long off,				// starting offset
					void *buf,				// buffer space for msg
					size_t buflen,			// length of buf
					void *cbarg);			// argument passed to callback
#endif

// return the name of a GCL
//		XXX: should this be in a more generic "getstat" function?
extern const gcl_name_t *gdp_gcl_getname(
					const gcl_handle_t *gclh);		// open GCL handle

// check to see if a GCL name is valid
extern bool		gdp_gcl_name_is_zero(const gcl_name_t);

// print a GCL (for debugging)
extern void		gdp_gcl_print(
					const gcl_handle_t *gclh,	// GCL handle to print
					FILE *fp,				// file to print it to
					int detail,				// not used at this time
					int indent);			// not used at this time

// print a GCL message (for debugging)
extern void		gdp_gcl_msg_print(
					const gdp_msg_t *msg,	// message to print
					FILE *fp);				// file to print it to

// make a printable GCL name from a binary version
char			*gdp_gcl_printable_name(
					const gcl_name_t internal,
					gcl_pname_t external);

// make a binary GCL name from a printable version
EP_STAT			gdp_gcl_internal_name(
					const gcl_pname_t external,
					gcl_name_t internal);

// allocate a new message
gdp_msg_t		*gdp_msg_new(void);

// free a message
void			gdp_msg_free(gdp_msg_t *);

#endif // _GDP_H_
