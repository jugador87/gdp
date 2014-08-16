/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	These headers are not intended for external use.
*/

#ifndef _GDP_PRIV_H_
#define _GDP_PRIV_H_

#include <ep/ep_thr.h>
#include <gdp/gdp_pkt.h>
#include <gdp/gdp_timestamp.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

#include <stdio.h>

extern pthread_t	_GdpIoEventLoopThread;

// declare the type of the gdp_req linked list (used multiple places)
LIST_HEAD(req_head, gdp_req);

struct gcl_handle
{
	// fields used by GDP library
	EP_THR_MUTEX		mutex;			// lock on this data structure
//	EP_THR_COND			cond;			// pthread wakeup signal
	struct req_head		reqs;			// list of outstanding requests
	gcl_name_t			gcl_name;		// the internal name
	gdp_iomode_t		iomode;			// read only or append only

	// transient (should only be used when locked)
	//XXX	subsumed into gdp_req
//	gdp_recno_t			recno;			// recno from last operation
//	gdp_buf_t			*revb;			// buffer for return results
//	uint32_t			flags;			// see below
//	EP_STAT				estat;			// status code from last operation
//	tt_interval_t		ts;				// timestamp from last operation

	// fields used only by gdpd
	long				ver;			// version number of on-disk file
	FILE				*fp;			// pointer to the on-disk file
	void				*log_index;
	off_t				data_offset;	// offset for end of header and start of data
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
**
**		XXX	In the future this should probably either incorporate or
**			replace gdp_msg_t.
*/

typedef struct gdp_req
{
	EP_THR_MUTEX		mutex;		// lock on this data structure
	EP_THR_COND			cond;		// pthread wakeup condition variable
	LIST_ENTRY(gdp_req)	list;		// linked list for cache management
	gcl_handle_t		*gclh;		// the corresponding GCL handle
	gdp_pkt_t			pkt;		// packet buffer
	EP_STAT				stat;		// status code from last operation
	uint32_t			flags;		// see below
	void				(*cb)(void *);	// callback (see above)
	void				*udata;		// user-supplied opaque data
} gdp_req_t;

#define GDP_REQ_DONE	0x00000001	// operation complete
#define GDP_REQ_SYNC	0x00000002	// request deleted after response
#define GDP_REQ_PERSIST	0x00000004	// request persists even after response
#define GDP_REQ_OWN_MSG	0x00000008	// we own the msg field (in pkt)

extern gdp_req_t	*_gdp_req_find(gcl_handle_t *gclh, gdp_rid_t rid);
extern gdp_rid_t	_gdp_rid_new(gcl_handle_t *gclh);

/*
**  Structure used for registering command functions
**
**		The names are already known to the GDP library, so this is just
**		to bind functions that implement the individual commands.
*/

typedef EP_STAT	cmdfunc_t(			// per command dispatch entry
					gdp_req_t *req,				// the request to be processed
					struct bufferevent *chan);	// the I/O channel (socket)

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

typedef struct bufferevent	gdp_chan_t;

#define GCL_NEXT_MSG	(-1)		// sentinel for next available message

gcl_handle_t	*_gdp_gcl_cache_get(
						gcl_name_t gcl_name,
						gdp_iomode_t mode);

void			_gdp_gcl_cache_add(
						gcl_handle_t *gclh,
						gdp_iomode_t mode);

void			_gdp_gcl_cache_drop(
						gcl_name_t gcl_name,
						gdp_iomode_t mode);

EP_STAT			_gdp_gcl_cache_init(void);

EP_STAT			_gdp_gcl_newhandle(gcl_name_t gcl_name,
						gcl_handle_t **gclhp);

void			_gdp_gcl_freehandle(gcl_handle_t *gclh);

void			_gdp_gcl_newname(
						gcl_name_t gcl_name);

const char		*_gdp_proto_cmd_name(uint8_t cmd);

EP_STAT			_gdp_start_event_loop_thread(
						pthread_t *thr,
						struct event_base *evb,
						const char *where);

EP_STAT			_gdp_do_init(bool run_event_loop);

EP_STAT			_gdp_req_new(int cmd,
						gcl_handle_t *gclh,
						uint32_t flags,
						gdp_req_t **reqp);

void			_gdp_req_free(gdp_req_t *req);

EP_STAT			_gdp_req_dispatch(gdp_req_t *req,
						gdp_chan_t *chan);

void			_gdp_req_freeall(struct req_head *reqlist);

void			_gdp_chan_drain_input(gdp_chan_t *chan);

EP_STAT			_gdp_invoke(int cmd,
						gcl_handle_t *gclh,
						gdp_msg_t *msg);

#endif // _GDP_PRIV_H_
