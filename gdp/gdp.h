/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP.H --- public headers for use of the Swarm Global Data Plane
*/

#ifndef _GDP_H_
#define _GDP_H_

#include <gdp/gdp_buf.h>
#include <gdp/gdp_stat.h>

#include <ep/ep.h>
#include <ep/ep_mem.h>
#include <ep/ep_stat.h>
#include <ep/ep_time.h>

#include <event2/event.h>
#include <event2/bufferevent.h>

#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/types.h>

/**********************************************************************
**	Opaque structures
*/

// an open handle on a GCL (opaque)
typedef struct gdp_gcl		gdp_gcl_t;

// GCL metadata
typedef struct gdp_gclmd	gdp_gclmd_t;
typedef uint32_t			gdp_gclmd_id_t;

// QoS requirements
typedef struct gdp_qos_req	gdp_qos_req_t;

/**********************************************************************
**	Other data types
*/

// the internal name of a GCL (256 bits)
typedef uint8_t				gdp_name_t[32];

#define GDP_NAME_SAME(a, b)	(memcmp((a), (b), sizeof (gdp_name_t)) == 0)

// the printable name of a GCL
#define GDP_GCL_PNAME_LEN	43			// length of an encoded pname
typedef char				gdp_pname_t[GDP_GCL_PNAME_LEN + 1];

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


/*
**  GCL Metadata keys
*/

#define GDP_GCLMD_XID		0x00584944	// XID (external id)
#define GDP_GCLMD_PUBKEY	0x00505542	// PUB (public key)
#define GDP_GCLMD_CTIME		0x0043544D	// CTM (creation time)
#define GDP_GCLMD_CID		0x00434944	// CID (creator id)


/*
**	 Datums
**		These are the underlying data unit that is passed through a GCL.
*/

typedef struct gdp_datum	gdp_datum_t;


/*
**	Events
**		gdp_event_t encodes an event.  Every event has a type and may
**		optionally have a GCL handle and/or a message.  For example,
**		data (from a subscription) has all three.
*/

typedef struct gdp_event	gdp_event_t;

// event types
#define _GDP_EVENT_FREE		0	// internal use: event is free
#define GDP_EVENT_DATA		1	// returned data
#define GDP_EVENT_EOS		2	// normal end of subscription
#define GDP_EVENT_SHUTDOWN	3	// subscription terminating because of shutdown
#define GDP_EVENT_ASTAT		4	// asynchronous status

extern gdp_event_t		*gdp_event_next(		// get event (caller must free!)
							gdp_gcl_t *gcl,			// if set wait for this GCL only
							EP_TIME_SPEC *timeout);

extern EP_STAT			gdp_event_free(			// free event from gdp_event_next
							gdp_event_t *gev);		// event to free

extern int				gdp_event_gettype(		// get the type of the event
							gdp_event_t *gev);

extern EP_STAT			gdp_event_getstat(		// get status code
							gdp_event_t *gev);

extern gdp_gcl_t		*gdp_event_getgcl(		// get the GCL of the event
							gdp_event_t *gev);

extern gdp_datum_t		*gdp_event_getdatum(	// get the datum of the event
							gdp_event_t *gev);

extern void				*gdp_event_getudata(	// get user data (callback only)
							gdp_event_t *gev);

typedef void			(*gdp_event_cbfunc_t)(	// the callback function
							gdp_event_t *ev);		// the event triggering the call

/**********************************************************************
**	Public globals and functions
*/

struct event_base		*GdpIoEventBase;	// the base for GDP I/O events

// initialize the library
extern EP_STAT	gdp_init(
					const char *gdpd_addr);	// address of gdpd

// run event loop (normally run from gdp_init; never returns)
extern void		*gdp_run_accept_event_loop(
					void *);				// unused

// create a new GCL
extern EP_STAT	gdp_gcl_create(
					gdp_name_t gclname,
					gdp_name_t logdname,
					gdp_gclmd_t *,			// pointer to metadata object
					gdp_gcl_t **pgcl);

// open an existing GCL
extern EP_STAT	gdp_gcl_open(
					gdp_name_t name,		// GCL name to open
					gdp_iomode_t rw,		// read/write (append)
					gdp_qos_req_t *qos,		// QoS requirements (placeholder)
					gdp_gcl_t **gcl);		// pointer to result GCL handle

// close an open GCL
extern EP_STAT	gdp_gcl_close(
					gdp_gcl_t *gcl);		// GCL handle to close

// append to a writable GCL
extern EP_STAT	gdp_gcl_append(
					gdp_gcl_t *gcl,			// writable GCL handle
					gdp_datum_t *);			// message to write

// async version
extern EP_STAT gdp_gcl_append_async(
					gdp_gcl_t *gcl,			// writable GCL handle
					gdp_datum_t *,			// message to write
					gdp_event_cbfunc_t,		// callback function
					void *udata);

// read from a readable GCL
extern EP_STAT	gdp_gcl_read(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_recno_t recno,		// GCL record number
					gdp_datum_t *datum);	// pointer to result message

// subscribe to a readable GCL
//	If you don't specific cbfunc, events are generated instead
typedef gdp_event_cbfunc_t	gdp_gcl_sub_cbfunc_t;	// back compat

extern EP_STAT	gdp_gcl_subscribe(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_recno_t start,		// first record to retrieve
					int32_t numrecs,		// number of records to retrieve
					EP_TIME_SPEC *timeout,	// timeout
					gdp_event_cbfunc_t cbfunc,
											// callback function for next datum
					void *cbarg);			// argument passed to callback

// read multiple records (no subscriptions)
extern EP_STAT	gdp_gcl_multiread(
					gdp_gcl_t *gcl,			// readable GCL handle
					gdp_recno_t start,		// first record to retrieve
					int32_t numrecs,		// number of records to retrieve
					gdp_event_cbfunc_t cbfunc,
											// callback function for next datum
					void *cbarg);			// argument passed to callback

// read metadata
extern EP_STAT	gdp_gcl_getmetadata(
					gdp_gcl_t *gcl,			// GCL handle
					gdp_gclmd_t **gmdp);	// out-param for metadata

// return the name of a GCL
//		XXX: should this be in a more generic "getstat" function?
extern const gdp_name_t *gdp_gcl_getname(
					const gdp_gcl_t *gcl);	// open GCL handle

// check to see if a GDP object name is valid
extern bool		gdp_name_is_valid(
					const gdp_name_t);

// print a GCL (for debugging)
extern void		gdp_gcl_print(
					const gdp_gcl_t *gcl,	// GCL handle to print
					FILE *fp);

// make a printable GDP object name from a binary version
char			*gdp_printable_name(
					const gdp_name_t internal,
					gdp_pname_t external);

// print an internal name for human use
void			gdp_print_name(
					const gdp_name_t internal,
					FILE *fp);

// make a binary GDP object name from a printable version
EP_STAT			gdp_internal_name(
					const gdp_pname_t external,
					gdp_name_t internal);

// parse a (possibly human-friendly) GDP object name
EP_STAT			gdp_parse_name(
					const char *ext,
					gdp_name_t internal);

// create a new metadata set
gdp_gclmd_t		*gdp_gclmd_new(
					int entries);

// free a metadata set
void			gdp_gclmd_free(gdp_gclmd_t *gmd);

// add an entry to a metadata set
EP_STAT			gdp_gclmd_add(
					gdp_gclmd_t *gmd,
					gdp_gclmd_id_t id,
					size_t len,
					const void *data);

// get an entry from a metadata set by index
EP_STAT			gdp_gclmd_get(
					gdp_gclmd_t *gmd,
					int indx,
					gdp_gclmd_id_t *id,
					size_t *len,
					const void **data);

// get an entry from a metadata set by id
EP_STAT			gdp_gclmd_find(
					gdp_gclmd_t *gmd,
					gdp_gclmd_id_t id,
					size_t *len,
					const void **data);

// print metadata set (for debugging)
void			gdp_gclmd_print(
					gdp_gclmd_t *gmd,
					FILE *fp,
					int detail);

// allocate a new message
gdp_datum_t		*gdp_datum_new(void);

// free a message
void			gdp_datum_free(gdp_datum_t *);

// print out data record
extern void		gdp_datum_print(
					const gdp_datum_t *datum,	// message to print
					FILE *fp,					// file to print it to
					uint32_t flags);			// formatting options

#define GDP_DATUM_PRTEXT	0x00000001		// print data as text
#define GDP_DATUM_PRDEBUG	0x00000002		// print debugging info

// get the record number from a datum
extern gdp_recno_t	gdp_datum_getrecno(
					const gdp_datum_t *datum);

// get the timestamp from a datum
extern void		gdp_datum_getts(
					const gdp_datum_t *datum,
					EP_TIME_SPEC *ts);

// get the data length from a datum
extern size_t	gdp_datum_getdlen(
					const gdp_datum_t *datum);

// get the data buffer from a datum
extern gdp_buf_t *gdp_datum_getbuf(
					const gdp_datum_t *datum);


#endif // _GDP_H_
