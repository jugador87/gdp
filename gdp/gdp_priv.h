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

typedef struct bufferevent	gdp_chan_t;

extern pthread_t		_GdpIoEventLoopThread;
gdp_chan_t				*_GdpChannel;	// our primary app-level protocol port

#include "gdp_pkt.h"

// declare the type of the gdp_req linked list (used multiple places)
LIST_HEAD(req_head, gdp_req);


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
	size_t				dlen;		// length of data buffer (redundant)
	gdp_buf_t			*dbuf;		// data buffer
};


/*
**  GDP Channel/Logs
*/

struct gdp_gcl
{
	EP_THR_MUTEX		mutex;			// lock on this data structure
//	EP_THR_COND			cond;			// pthread wakeup signal
	struct req_head		reqs;			// list of outstanding requests
	gcl_name_t			gcl_name;		// the internal name
	gcl_pname_t			pname;			// printable name (for debugging)
	gdp_iomode_t		iomode;			// read only or append only
	int					refcnt;			// reference counter
	struct gdp_gcl_xtra	*x;				// for use by gdpd, gdp-rest
};


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
**		The cb (callback) field is used primarily by GDPD to
**		indicate the routine that should be called in a worker
**		thread.  The callback gets the request as the parameter.
*/

typedef struct gdp_req
{
	EP_THR_MUTEX		mutex;		// lock on this data structure
	EP_THR_COND			cond;		// pthread wakeup condition variable
	LIST_ENTRY(gdp_req)	list;		// linked list for cache management
	bool				inuse:1;	// indicates request is allocated
	bool				postproc:1;	// invoke callback for late cmd processing
	gdp_gcl_t			*gclh;		// the corresponding GCL handle
	gdp_pkt_t			*pkt;		// packet buffer
	gdp_chan_t			*chan;		// the network channel for this req
	EP_STAT				stat;		// status code from last operation
	int32_t				numrecs;	// remaining number of records to return
	uint32_t			flags;		// see below
	void				(*cb)(struct gdp_req *);
									// callback (see above)
	void				*udata;		// user-supplied opaque data
} gdp_req_t;

#define GDP_REQ_DONE			0x00000001	// operation complete
#define GDP_REQ_SUBSCRIPTION	0x00000002	// this is a subscription
#define GDP_REQ_PERSIST			0x00000004	// request persists after response
#define GDP_REQ_SUBUPGRADE		0x00000008	// can upgrade to subscription

extern gdp_req_t	*_gdp_req_find(gdp_gcl_t *gclh, gdp_rid_t rid);
extern gdp_rid_t	_gdp_rid_new(gdp_gcl_t *gclh);


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

void			_gdp_register_cmdfuncs(struct cmdfuncs *);

// callback info passed into event loops
struct event_loop_info
{
	struct event_base	*evb;		// the event base for this loop
	const char			*where;		// a name to convey in errors
};

#define GCL_NEXT_MSG	(-1)		// sentinel for next available message

gdp_gcl_t		*_gdp_gcl_cache_get(
						gcl_name_t gcl_name,
						gdp_iomode_t mode);

void			_gdp_gcl_cache_add(
						gdp_gcl_t *gclh,
						gdp_iomode_t mode);

void			_gdp_gcl_cache_drop(
						gdp_gcl_t *gclh);

EP_STAT			_gdp_gcl_cache_init(void);

void			_gdp_gcl_incref(
						gdp_gcl_t *gclh);

void			_gdp_gcl_decref(
						gdp_gcl_t *gclh);

EP_STAT			_gdp_gcl_newhandle(gcl_name_t gcl_name,
						gdp_gcl_t **gclhp);

void			_gdp_gcl_freehandle(gdp_gcl_t *gclh);

void			_gdp_gcl_newname(
						gcl_name_t gcl_name);

const char		*_gdp_proto_cmd_name(uint8_t cmd);

EP_STAT			_gdp_start_event_loop_thread(
						pthread_t *thr,
						struct event_base *evb,
						const char *where);

EP_STAT			_gdp_do_init(bool run_event_loop);

EP_STAT			_gdp_req_new(int cmd,
						gdp_gcl_t *gclh,
						gdp_chan_t *chan,
						uint32_t flags,
						gdp_req_t **reqp);

void			_gdp_req_free(gdp_req_t *req);

EP_STAT			_gdp_req_dispatch(gdp_req_t *req);

void			_gdp_req_freeall(struct req_head *reqlist);

void			_gdp_req_dump(gdp_req_t *req,
						FILE *fp);

void			_gdp_chan_drain_input(gdp_chan_t *chan);

EP_STAT			_gdp_invoke(gdp_req_t *req);

#endif // _GDP_PRIV_H_
