/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	These headers are not intended for external use.
*/

#ifndef _GDP_PRIV_H_
#define _GDP_PRIV_H_

#include <ep/ep_thr.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

#include <stdio.h>

typedef struct gdp_chan		gdp_chan_t;
typedef struct gcl_class	gcl_class_t;

extern pthread_t	_GdpIoEventLoopThread;
gdp_chan_t			*_GdpChannel;		// our primary app-level protocol port
gdp_name_t			_GdpMyRoutingName;	// source name for PDUs

#include "gdp_pdu.h"

// declare the type of the gdp_req linked list (used multiple places)
LIST_HEAD(req_head, gdp_req);


struct gdp_chan
{
	EP_THR_MUTEX		mutex;			// lock before changes
	EP_THR_COND			cond;			// wake up after state change
	int16_t				state;			// current state of channel
	struct bufferevent	*bev;			// associated bufferevent (socket)
	struct req_head		reqs;			// reqs associated with this channel
	gdp_name_t			serverid;		// the name of the server (random nonce)
	void				(*close_cb)(	// called on channel close
							gdp_chan_t *chan);
	EP_STAT				(*advertise)(	// called to do our advertisements
							void);
	void				(*process)(		// called to process a packet
							gdp_pdu_t *pdu,
							gdp_chan_t *chan);
};

/* Channel states */
#define GDP_CHAN_UNCONNECTED	0		// channel is not connected yet
#define GDP_CHAN_CONNECTING		1		// connection being initiated
#define GDP_CHAN_CONNECTED		2		// channel is connected and active
#define GDP_CHAN_ERROR			3		// channel has had error
#define GDP_CHAN_CLOSING		4		// channel is closing


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

struct gdp_datum
{
	EP_THR_MUTEX		mutex;		// locking mutex (mostly for dbuf)
	struct gdp_datum	*next;		// next in free list
	bool				inuse:1;	// indicates that the datum is in use
	gdp_recno_t			recno;		// the record number
	EP_TIME_SPEC		ts;			// timestamp for this message
	gdp_buf_t			*dbuf;		// data buffer
};


/*
**  GDP Channel/Logs
**
**		The implementation class is currently used only by gdpd,
**		to distinguish between GCLs that are locally stored and
**		those available remotely.
*/

struct gdp_gcl
{
	EP_THR_MUTEX		mutex;			// lock on this data structure
//	EP_THR_COND			cond;			// pthread wakeup signal
	struct req_head		reqs;			// list of outstanding requests
	gdp_name_t			name;			// the internal name
	gdp_pname_t			pname;			// printable name (for debugging)
	gdp_iomode_t		iomode;			// read only or append only
	uint16_t			flags;			// flag bits, see below
	int					refcnt;			// reference counter
	void				(*freefunc)(struct gdp_gcl *);
										// called when this is freed
	gcl_class_t			*cl;			// implementation class
	void				*cd;			// class-specific data
	struct gdp_gcl_xtra	*x;				// for use by gdpd, gdp-rest
};

/* flags for GCL handles */
#define GCLF_DROPPING	0x0001		// handle is being deallocated
#define GCLF_INCACHE	0x0002		// handle is in cache


/*
**  A Work Request (and associated Response)
**
**		A GDP request is packaged up in one of these things and
**		submitted.  Responses are returned in the same structure.
**
**		There is one packet header here that is used both for
**		sending and receiving.  The associated gdp_buf_t does not
**		have a write callback, so it can be used without having
**		any side effects.  The packet header has much of the
**		information, including the command/response code, the rid,
**		the record number, the timestamp, and the data buffer.
**
**		There can be mulitple requests active on a single GCL at
**		any time, but they should have unique rids.  Rids can be
**		reused if desired once an operation is complete.  Note:
**		some operations (e.g., subscriptions) can return multiple
**		results, but they will have the same rid.
**
**		The cb (callback) field is used by GDPD to indicate the
**		routine that should be called in a worker thread.  The
**		callback gets the request as the parameter.  It is also
**		used in the gdp library for subscription callbacks.
*/

typedef struct gdp_req
{
	EP_THR_MUTEX		mutex;		// lock on this data structure
	EP_THR_COND			cond;		// pthread wakeup condition variable
	LIST_ENTRY(gdp_req)	gcllist;	// linked list for cache management
	LIST_ENTRY(gdp_req)	chanlist;	// reqs associated with a given channel
	bool				inuse:1;	// indicates request is allocated
	bool				postproc:1;	// invoke callback for late cmd processing
	bool				ongcllist:1;	// this is on a gcl list
	bool				onchanlist:1;	// this is on a channel list
	gdp_gcl_t			*gcl;		// the corresponding GCL handle
	gdp_pdu_t			*pdu;		// packet buffer
	gdp_chan_t			*chan;		// the network channel for this req
	EP_STAT				stat;		// status code from last operation
	int32_t				numrecs;	// remaining number of records to return
	uint32_t			flags;		// see below
	union
	{
		void					(*gdpd)(struct gdp_req *);
		gdp_gcl_sub_cbfunc_t	subs;
		void					(*generic)(void *);
	}					cb;			// callback (see above)
	void				*udata;		// user-supplied opaque data to cb
} gdp_req_t;

#define GDP_REQ_DONE			0x00000001	// operation complete
#define GDP_REQ_SUBSCRIPTION	0x00000002	// this is a subscription
#define GDP_REQ_PERSIST			0x00000004	// request persists after response
#define GDP_REQ_SUBUPGRADE		0x00000008	// can upgrade to subscription
#define GDP_REQ_ALLOC_RID		0x00000010	// force allocation of new rid

extern gdp_req_t	*_gdp_req_find(gdp_gcl_t *gcl, gdp_rid_t rid);
extern gdp_rid_t	_gdp_rid_new(gdp_gcl_t *gcl, gdp_chan_t *chan);
extern EP_STAT		_gdp_req_send(gdp_req_t *req);


/*
**  Structure used for registering command functions
**
**		The names are already known to the GDP library, so this is just
**		to bind functions that implement the individual commands.
*/

typedef EP_STAT	cmdfunc_t(			// per command dispatch entry
					gdp_req_t *req);	// the request to be processed

struct cmdfuncs
{
	int			cmd;				// command number
	cmdfunc_t	*func;				// pointer to implementing function
};

void			_gdp_register_cmdfuncs(
						struct cmdfuncs *);

const char		*_gdp_proto_cmd_name(		// return printable cmd name
						uint8_t cmd);

/*
**  GCL cache.
**
**		Implemented in gdp/gdp_gcl_cache.c.
*/

EP_STAT			_gdp_gcl_cache_init(void);	// initialize cache

gdp_gcl_t		*_gdp_gcl_cache_get(		// get entry from cache
						gdp_name_t gcl_name,
						gdp_iomode_t mode);

void			_gdp_gcl_cache_add(			// add entry to cache
						gdp_gcl_t *gcl,
						gdp_iomode_t mode);

void			_gdp_gcl_cache_drop(		// drop entry from cache
						gdp_gcl_t *gcl);

void			_gdp_gcl_incref(			// increase reference count
						gdp_gcl_t *gcl);

void			_gdp_gcl_decref(			// decrease reference count
						gdp_gcl_t *gcl);

/*
**  Other GCL handling.  These are shared between client access
**  and the GDP daemon.
**
**  Implemented in gdp/gdp_gcl_ops.c.
*/

EP_STAT			_gdp_gcl_newhandle(			// create new in-mem handle
						gdp_name_t gcl_name,
						gdp_gcl_t **gclhp);

void			_gdp_gcl_freehandle(		// free in-memory handle
						gdp_gcl_t *gcl);

void			_gdp_gcl_newname(			// create a new name
						gdp_gcl_t *gcl);

EP_STAT			_gdp_gcl_create(			// create a new GCL
						gdp_name_t gclname,
						gdp_name_t logdname,
						gdp_gclmd_t *gmd,
						gdp_chan_t *chan,
						uint32_t reqflags,
						gdp_gcl_t **pgcl);

EP_STAT			_gdp_gcl_open(				// open a GCL
						gdp_gcl_t *gcl,
						int cmd,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_close(				// close a GCL (handle)
						gdp_gcl_t *gcl,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_read(				// read a GCL record (gdpd shared)
						gdp_gcl_t *gcl,
						gdp_datum_t *datum,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_publish(			// publish a record (gdpd shared)
						gdp_gcl_t *gcl,
						gdp_datum_t *datum,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_subscribe(			// subscribe to data
						gdp_gcl_t *gcl,
						int cmd,
						gdp_recno_t start,
						int32_t numrecs,
						EP_TIME_SPEC *timeout,
						gdp_gcl_sub_cbfunc_t cbfunc,
						void *cbarg,
						gdp_chan_t *chan,
						uint32_t reqflags);

EP_STAT			_gdp_gcl_getmetadata(		// retrieve metadata
						gdp_gcl_t *gcl,
						gdp_gclmd_t **gmdp,
						gdp_chan_t *chan,
						uint32_t reqflags);

/*
**  Initialization.
*/

EP_STAT			_gdp_start_event_loop_thread(
						pthread_t *thr,
						struct event_base *evb,
						const char *where);

void			_gdp_newname(gdp_name_t gname);

EP_STAT			_gdp_lib_init(void);

/*
**  Request handling.
*/

EP_STAT			_gdp_req_new(				// create new request
						int cmd,
						gdp_gcl_t *gcl,
						gdp_chan_t *chan,
						gdp_pdu_t *pdu,
						uint32_t flags,
						gdp_req_t **reqp);

void			_gdp_req_free(				// free old request
						gdp_req_t *req);

EP_STAT			_gdp_req_dispatch(			// do local req processing
						gdp_req_t *req);

EP_STAT			_gdp_invoke(				// send request to daemon
						gdp_req_t *req);

void			_gdp_req_freeall(			// free all requests in list
						struct req_head *reqlist);

void			_gdp_req_dump(				// print (debug) request
						gdp_req_t *req,
						FILE *fp);

void			_gdp_chan_drain_input(		// drain all input from channel
						gdp_chan_t *chan);

EP_STAT			_gdp_advertise(				// advertise resources (generic)
						EP_STAT (*func)(gdp_buf_t *, void *),
						void *ctx);

EP_STAT			_gdp_advertise_me(			// advertise me only
						void);

/*
**  Gdpd communication
*/

EP_STAT			_gdp_chan_open(				// open channel to gdpd
						const char *gdpd_addr,
						void (*process)(gdp_pdu_t *, gdp_chan_t *),
						gdp_chan_t **pchan);
void			_gdp_chan_close(			// close channel
						gdp_chan_t **pchan);

/*
**  Libevent support
*/

// callback info passed into event loops
struct event_loop_info
{
	struct event_base	*evb;		// the event base for this loop
	const char			*where;		// a name to convey in errors
};

EP_STAT			_gdp_evloop_init(void);		// start event loop

#endif // _GDP_PRIV_H_
