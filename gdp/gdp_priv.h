/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	These headers are not intended for external use.
*/

#ifndef _GDP_PRIV_H_
#define _GDP_PRIV_H_

#include <gdp/gdp_timestamp.h>
#include <ep/ep_thr.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

#include <stdio.h>

extern pthread_t	_GdpIoEventLoopThread;

// declare the type of the linked list (used multiple places)
LIST_HEAD(req_list, gdp_req);

struct gcl_handle
{
	// fields used by GDP library
	EP_THR_MUTEX		mutex;			// lock on this data structure
//	EP_THR_COND			cond;			// pthread wakeup signal
	struct req_list		reqs;			// list of outstanding requests
	gcl_name_t			gcl_name;		// the internal name
	gdp_iomode_t		iomode;			// read only or append only

	// transient (should only be used when locked)
	//XXX	subsumed into gdp_req
//	gdp_msgno_t			msgno;			// msgno from last operation
//	gdp_buf_t			*revb;			// buffer for return results
//	uint32_t			flags;			// see below
//	EP_STAT				estat;			// status code from last operation
//	tt_interval_t		ts;				// timestamp from last operation

	// fields used only by gdpd
	long				ver;			// version number of on-disk file
	FILE				*fp;			// pointer to the on-disk file
//	off_t				*offcache;		// offsets of records we have seen
//	long				cachesize;		// size of offcache array
//	gdp_msgno_t			maxmsgno;		// last msgno that we have read/written
	void				*log_index;
	off_t				data_offset;	// offset for end of header and start of data
};


/*
**  A Work Request (and associated Response)
**
**		A GDP request is packaged up in one of these things and
**		submitted.  Responses are returned in the same structure.
**
**		There can be mulitple requests active on a single GCL at
**		any time, but they should have unique rids.  Rids can be
**		reused if desired once an operation is complete.  Note:
**		some operations (e.g., subscriptions) can return multiple
**		results, but they will have the same rid.
**
**		XXX	In the future this should probably either incorporate or
**			replace gdp_msg_t.
*/

typedef struct gdp_req
{
	EP_THR_MUTEX		mutex;		// lock on this data structure
	EP_THR_COND			cond;		// pthread wakeup condition variable
	LIST_ENTRY(gdp_req)	list;		// linked list for cache management
	int					cmd;		// the command being issued
	gdp_rid_t			rid;		// the corresponding request id
	gcl_handle_t		*gclh;		// the corresponding GCL handle
	gdp_msgno_t			recno;		// the record number (if dealing with data)
	tt_interval_t		ts;			// the record timestamp
	size_t				dlen;		// length of data buffer
	gdp_buf_t			*dbuf;		// data buffer
	EP_STAT				stat;		// status code from last operation
	uint32_t			flags;		// see below
	void				*udata;		// user-supplied opaque data
} gdp_req_t;

#define GDP_REQ_DONE	0x00000001	// operation complete
#define GDP_REQ_SYNC	0x00000002	// request deleted after response
#define GDP_REQ_PERSIST	0x00000004	// request persists even after response

#define GCL_NEXT_MSG	(-1)			// sentinel for next available message


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

const char		*_gdp_proto_cmd_name(uint8_t cmd);

EP_STAT			_gdp_start_event_loop_thread(
						struct event_base *evb);

void			_gdp_gcl_newname(
						gcl_name_t gcl_name);

EP_STAT			_gdp_start_accept_event_loop_thread(
						struct event_base *evb);

EP_STAT			_gdp_start_io_event_loop_thread(
						struct event_base *evb);

EP_STAT			_gdp_do_init(bool run_event_loop);

EP_STAT			_gdp_gcl_newhandle(gcl_name_t gcl_name,
						gcl_handle_t **gclhp);

void			_gdp_gcl_freehandle(gcl_handle_t *gclh);

void			_gdp_req_freeall(struct req_list *reqlist);

EP_STAT			_gdp_invoke(int cmd,
						gcl_handle_t *gclh,
						gdp_msg_t *msg);

#endif // _GDP_PRIV_H_
