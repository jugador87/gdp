/* vim: set ai sw=4 sts=4 ts=4 : */

#include "gdpd.h"
#include "gdpd_physlog.h"
#include "gdpd_pubsub.h"
#include "thread_pool.h"

#include <ep/ep_string.h>
#include <ep/ep_hexdump.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>

#include <errno.h>
#include <arpa/inet.h>


/************************  PRIVATE	************************/

static EP_DBG Dbg = EP_DBG_INIT("gdp.gdpd", "GDP Daemon");

static struct thread_pool *CpuJobThreadPool;


/*
**	GDPD_GCL_ERROR --- helper routine for returning errors
*/

EP_STAT
gdpd_gcl_error(gcl_name_t gcl_name, char *msg, EP_STAT logstat, int nak)
{
	gcl_pname_t pname;

	gdp_gcl_printable_name(gcl_name, pname);
	gdp_log(logstat, "%s: %s", msg, pname);
	return GDP_STAT_NAK_BADREQ;
}

void
flush_input_data(gdp_req_t *req, char *where)
{
	int i;

	if (req->pkt->datum != NULL &&
			(i = evbuffer_get_length(req->pkt->datum->dbuf)) > 0)
	{
		ep_dbg_cprintf(Dbg, 4,
				"flush_input_data: %s: flushing %d bytes of unexpected input\n",
				where, i);
		gdp_buf_reset(req->pkt->datum->dbuf);
	}
}

/*
**	Command implementations
*/

EP_STAT
implement_me(char *s)
{
	ep_app_error("Not implemented: %s", s);
	return GDP_STAT_NOT_IMPLEMENTED;
}


/***********************************************************************
**  GDP command implementations
**
**		Each of these takes a request as the argument.
**
**		These routines should set req->pkt->cmd to the "ACK" reply
**		code, which will be used if the command succeeds (i.e.,
**		returns EP_STAT_OK).  Otherwise the return status is decoded
**		to produce a NAK code.  A specific NAK code can be sent
**		using GDP_STAT_FROM_NAK(nak).
**
**		All routines are expected to consume all their input from
**		the channel and to write any output to the same channel.
**		They can consume any unexpected input using flush_input_data.
**
***********************************************************************/

/*
**  CMD_CREATE --- create new GCL.
*/

EP_STAT
cmd_create(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_gcl_t *gclh;

	req->pkt->cmd = GDP_ACK_CREATED;

	// no input, so we can reset the buffer just to be safe
	flush_input_data(req, "cmd_create");

	// get the memory space
	estat = gcl_alloc(req->pkt->gcl_name, GDP_MODE_AO, &gclh);
	EP_STAT_CHECK(estat, goto fail0);
	req->gclh = gclh;			// for debugging

	// do the physical create
	estat = gcl_physcreate(req->pkt->gcl_name, gclh);
	EP_STAT_CHECK(estat, goto fail1);

	// cache the open GCL Handle for possible future use
	EP_ASSERT_INSIST(!gdp_gcl_name_is_zero(gclh->gcl_name));
	_gdp_gcl_cache_add(gclh, GDP_MODE_AO);

	// pass any creation info back to the caller
	// (none at this point)

	// release resources
	_gdp_gcl_decref(gclh);
	req->gclh = NULL;
	return estat;

fail1:
	_gdp_gcl_freehandle(gclh);

fail0:
	return estat;
}


/*
**  CMD_OPEN_RO, _AO --- open for read or append only
*/

EP_STAT
cmd_open_xx(gdp_req_t *req, gdp_iomode_t iomode)
{
	EP_STAT estat = EP_STAT_OK;

	req->pkt->cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_open_xx");

	// see if we already know about this GCL
	estat = get_open_handle(req, iomode);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pkt->gcl_name, "cmd_openxx: could not open GCL",
							estat, GDP_NAK_C_BADREQ);
	}

	req->pkt->datum->recno = gcl_max_recno(req->gclh);
	_gdp_gcl_decref(req->gclh);
	req->gclh = NULL;
	return estat;
}

EP_STAT
cmd_open_ao(gdp_req_t *req)
{
	return cmd_open_xx(req, GDP_MODE_AO);
}

EP_STAT
cmd_open_ro(gdp_req_t *req)
{
	return cmd_open_xx(req, GDP_MODE_RO);
}


/*
**  CMD_CLOSE --- close an open GCL
**
**		XXX	Since GCLs are shared between clients you really can't just
**			close things willy-nilly.  Thus, this is currently a no-op
**			until such time as reference counting works.
**
**		XXX	We need to have a way of expiring unused GCLs that are not
**			closed.
*/

EP_STAT
cmd_close(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;

	req->pkt->cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_close");

	// a bit wierd to open the GCL only to close it again....
	estat = get_open_handle(req, GDP_MODE_ANY);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pkt->gcl_name, "cmd_close: GCL not open",
							estat, GDP_NAK_C_BADREQ);
	}
	req->pkt->datum->recno = gcl_max_recno(req->gclh);
	_gdp_gcl_decref(req->gclh);
	req->gclh = NULL;

	return estat;
}


/*
**  CMD_READ --- read a single record from a GCL
**
**		This returns the data as part of the response.  To get multiple
**		values in one call, see cmd_subscribe.
*/

EP_STAT
cmd_read(gdp_req_t *req)
{
	EP_STAT estat;

	req->pkt->cmd = GDP_ACK_CONTENT;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_read");

	estat = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pkt->gcl_name, "cmd_read: GCL open failure",
							estat, GDP_NAK_C_BADREQ);
	}

	gdp_buf_reset(req->pkt->datum->dbuf);
	estat = gcl_physread(req->gclh, req->pkt->datum);

	if (EP_STAT_IS_SAME(estat, EP_STAT_END_OF_FILE))
		estat = GDP_STAT_NAK_NOTFOUND;
	_gdp_gcl_decref(req->gclh);
	req->gclh = NULL;
	return estat;
}


/*
**  CMD_PUBLISH --- publish (write) a datum to a GCL
**
**		This will have side effects if there are subscriptions pending.
*/

EP_STAT
cmd_publish(gdp_req_t *req)
{
	EP_STAT estat;

	req->pkt->cmd = GDP_ACK_CREATED;

	estat = get_open_handle(req, GDP_MODE_AO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pkt->gcl_name, "cmd_publish: GCL not open",
							estat, GDP_NAK_C_BADREQ);
	}

	// create the message
	estat = gcl_physappend(req->gclh, req->pkt->datum);

	// send the new data to any subscribers
	sub_notify_all_subscribers(req);

	// we can now let the data in the request go
	evbuffer_drain(req->pkt->datum->dbuf,
			evbuffer_get_length(req->pkt->datum->dbuf));

	_gdp_gcl_decref(req->gclh);
	req->gclh = NULL;

	return estat;
}


/*
**  POST_SUBSCRIBE --- do subscription work after initial ACK
**
**		Assuming the subscribe worked we are now going to deliver any
**		previously existing records.  Once those are all sent we can
**		convert this to an ordinary subscription.  If the subscribe
**		request is satisified, we remove it.
*/

void
post_subscribe(gdp_req_t *req)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 38,
			"post_subscribe: numrecs = %d, recno = %"PRIgdp_recno"\n",
			req->numrecs, req->pkt->datum->recno);

	// make sure the request has the right command
	req->pkt->cmd = GDP_ACK_CONTENT;

	while (req->numrecs >= 0)
	{
		// see if data pre-exists in the GCL
		if (req->pkt->datum->recno > gcl_max_recno(req->gclh))
		{
			// no, it doesn't; convert to long-term subscription
			break;
		}

		// get the next record and return it as an event
		estat = gcl_physread(req->gclh, req->pkt->datum);
		if (EP_STAT_ISOK(estat))
		{
			// OK, the next record exists: send it
			req->stat = estat = _gdp_pkt_out(req->pkt,
										bufferevent_get_output(req->chan));

			// have to clear the old data
			evbuffer_drain(req->pkt->datum->dbuf,
					evbuffer_get_length(req->pkt->datum->dbuf));

			// advance to the next record
			if (req->numrecs > 0 && --req->numrecs == 0)
			{
				// numrecs was positive, now zero, but zero means infinity
				req->numrecs--;
			}
			req->pkt->datum->recno++;
		}
		else if (!EP_STAT_IS_SAME(estat, EP_STAT_END_OF_FILE))
		{
			// this is some error that should be logged
			gdp_log(estat, "post_subscribe: bad read");
			req->numrecs = -1;		// terminate subscription
		}
		else
		{
			// shouldn't happen
			gdp_log(estat, "post_subscribe: read EOF");
		}

		// if we didn't successfully send a record, terminate
		EP_STAT_CHECK(estat, break);
	}

	if (req->numrecs < 0 || !EP_UT_BITSET(GDP_REQ_SUBUPGRADE, req->flags))
	{
		// no more to read: do cleanup & send termination notice
		sub_end_subscription(req);
	}
	else
	{
		ep_dbg_cprintf(Dbg, 24, "post_subscribe: converting to subscription\n");
		req->flags |= GDP_REQ_SUBSCRIPTION;

		// link this request into the GCL so the subscription can be found
		ep_thr_mutex_lock(&req->gclh->mutex);
		LIST_INSERT_HEAD(&req->gclh->reqs, req, list);
		ep_thr_mutex_unlock(&req->gclh->mutex); }
}


/*
**  CMD_SUBSCRIBE --- subscribe command
**
**		Arranges to return existing data (if any) after the response
**		is sent, and non-existing data (if any) as a side-effect of
**		publish.
**
**		XXX	Race Condition: if records are written between the time
**			the subscription and the completion of the first half of
**			this process, some records may be lost.  For example,
**			if the GCL has 20 records (1-20) and you ask for 20
**			records starting at record 11, you probably want records
**			11-30.  But if during the return of records 11-20 another
**			record (21) is written, then the second half of the
**			subscription will actually return records 22-31.
**
**		XXX	Does not implement timeouts.
*/

EP_STAT
cmd_subscribe(gdp_req_t *req)
{
	EP_STAT estat;
	EP_TIME_SPEC timeout;

	req->pkt->cmd = GDP_ACK_SUCCESS;

	// find the GCL handle
	estat = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pkt->gcl_name, "cmd_subscribe: GCL not open",
							estat, GDP_NAK_C_BADREQ);
	}

	// get the additional parameters: number of records and timeout
	req->numrecs = (int) gdp_buf_get_uint32(req->pkt->datum->dbuf);
	gdp_buf_get_timespec(req->pkt->datum->dbuf, &timeout);

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_subscribe: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				req->pkt->datum->recno, req->numrecs);
		gdp_gcl_print(req->gclh, ep_dbg_getfile(), 1, 0);
	}

	// should have no more input data; ignore anything there
	flush_input_data(req, "cmd_subscribe");

	if (req->numrecs < 0)
	{
		req->pkt->cmd = GDP_NAK_C_BADOPT;
		return GDP_STAT_FROM_C_NAK(req->pkt->cmd);
	}

	// mark this as persistent and upgradable
	req->flags |= GDP_REQ_PERSIST | GDP_REQ_SUBUPGRADE;

	// get our starting point, which may be relative to the end
	if (req->pkt->datum->recno <= 0)
	{
		req->pkt->datum->recno += gcl_max_recno(req->gclh) + 1;
		if (req->pkt->datum->recno <= 0)
		{
			// still starts before beginning; start from beginning
			req->pkt->datum->recno = 1;
		}
	}

	ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: starting from %" PRIgdp_recno
			", %d records\n",
			req->pkt->datum->recno, req->numrecs);

	// if some of the records already exist, arrange to return them
	if (req->pkt->datum->recno <= gcl_max_recno(req->gclh))
	{
		ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: doing post processing\n");
		req->cb = &post_subscribe;
		req->postproc = true;
	}
	else
	{
		// this is a pure "future" subscription
		ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: enabling subscription\n");
		req->flags |= GDP_REQ_SUBSCRIPTION;

		// link this request into the GCL so the subscription can be found
		ep_thr_mutex_lock(&req->gclh->mutex);
		LIST_INSERT_HEAD(&req->gclh->reqs, req, list);
		ep_thr_mutex_unlock(&req->gclh->mutex);
	}

	// we don't drop the GCL reference until the subscription is satisified

	return EP_STAT_OK;
}


/*
**  CMD_MULTIREAD --- read multiple records
**
**		Arranges to return existing data (if any) after the response
**		is sent.  No long-term subscription will ever be created, but
**		much of the infrastructure is reused.
*/

EP_STAT
cmd_multiread(gdp_req_t *req)
{
	EP_STAT estat;

	req->pkt->cmd = GDP_ACK_SUCCESS;

	// find the GCL handle
	estat  = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pkt->gcl_name, "cmd_multiread: GCL not open",
							estat, GDP_NAK_C_BADREQ);
	}

	// get the additional parameters: number of records and timeout
	req->numrecs = (int) gdp_buf_get_uint32(req->pkt->datum->dbuf);

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_multiread: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				req->pkt->datum->recno, req->numrecs);
		gdp_gcl_print(req->gclh, ep_dbg_getfile(), 1, 0);
	}

	// should have no more input data; ignore anything there
	flush_input_data(req, "cmd_multiread");

	// get our starting point, which may be relative to the end
	if (req->pkt->datum->recno <= 0)
	{
		req->pkt->datum->recno += gcl_max_recno(req->gclh) + 1;
		if (req->pkt->datum->recno <= 0)
		{
			// still starts before beginning; start from beginning
			req->pkt->datum->recno = 1;
		}
	}

	if (req->numrecs < 0)
	{
		req->pkt->cmd = GDP_NAK_C_BADOPT;
		return GDP_STAT_FROM_C_NAK(req->pkt->cmd);
	}

	// get our starting point, which may be relative to the end
	if (req->pkt->datum->recno <= 0)
	{
		req->pkt->datum->recno += gcl_max_recno(req->gclh) + 1;
		if (req->pkt->datum->recno <= 0)
		{
			// still starts before beginning; start from beginning
			req->pkt->datum->recno = 1;
		}
	}

	ep_dbg_cprintf(Dbg, 24, "cmd_multiread: starting from %" PRIgdp_recno
			", %d records\n",
			req->pkt->datum->recno, req->numrecs);

	// if some of the records already exist, arrange to return them
	if (req->pkt->datum->recno <= gcl_max_recno(req->gclh))
	{
		ep_dbg_cprintf(Dbg, 24, "cmd_multiread: doing post processing\n");
		req->cb = &post_subscribe;
		req->postproc = true;

		// make this a "snapshot", i.e., don't read additional records
		int32_t nrec = gcl_max_recno(req->gclh) - req->pkt->datum->recno;
		if (nrec < req->numrecs || req->numrecs == 0)
			req->numrecs = nrec + 1;

		// keep the request around until the post-processing is done
		req->flags |= GDP_REQ_PERSIST;
	}
	else
	{
		// this is a pure "future" subscription
		ep_dbg_cprintf(Dbg, 24, "cmd_multiread: enabling subscription\n");
		req->flags |= GDP_REQ_SUBSCRIPTION;

		// link this request into the GCL so the subscription can be found
		ep_thr_mutex_lock(&req->gclh->mutex);
		LIST_INSERT_HEAD(&req->gclh->reqs, req, list);
		ep_thr_mutex_unlock(&req->gclh->mutex);
	}

	return EP_STAT_OK;
}


/*
**  CMD_UNSUBSCRIBE --- terminate a subscription
**
**		XXX not implemented yet
*/


/*
**  CMD_NOT_IMPLEMENTED --- generic "not implemented" error
*/

EP_STAT
cmd_not_implemented(gdp_req_t *req)
{
	//XXX print/log something here?

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_not_implemented");

	req->pkt->cmd = GDP_NAK_S_NOTIMPL;
	_gdp_pkt_out_hard(req->pkt, bufferevent_get_output(req->chan));
	return GDP_STAT_NAK_NOTIMPL;
}


/**************** END OF COMMAND IMPLEMENTATIONS ****************/


/*
**	DISPATCH_CMD --- dispatch a command via the DispatchTable
**
**	Parameters:
**		req --- the request to be satisified
**		conn --- the connection (socket) handle
*/

EP_STAT
dispatch_cmd(gdp_req_t *req)
{
	EP_STAT estat;
	int cmd = req->pkt->cmd;

	if (ep_dbg_test(Dbg, 18))
	{
		ep_dbg_printf("dispatch_cmd: >>> command %d (%s)\n",
				cmd, _gdp_proto_cmd_name(cmd));
		if (ep_dbg_test(Dbg, 30))
			_gdp_req_dump(req, ep_dbg_getfile());
	}

	estat = _gdp_req_dispatch(req);

	// decode return status as an ACK/NAK command
	if (!EP_STAT_ISOK(estat))
	{
		req->pkt->cmd = GDP_NAK_S_INTERNAL;
		if (EP_STAT_REGISTRY(estat) == EP_REGISTRY_UCB &&
			EP_STAT_MODULE(estat) == GDP_MODULE)
		{
			int d = EP_STAT_DETAIL(estat);

			if (d >= 400 && d < (400 + GDP_NAK_C_MAX - GDP_NAK_C_MIN))
				req->pkt->cmd = d - 400 + GDP_NAK_C_MIN;
			else if (d >= 500 && d < (500 + GDP_NAK_S_MAX - GDP_NAK_S_MIN))
				req->pkt->cmd = d - 500 + GDP_NAK_S_MIN;
			else
				req->pkt->cmd = GDP_NAK_S_INTERNAL;
		}
	}

	if (ep_dbg_test(Dbg, 18))
	{
		char ebuf[200];

		ep_dbg_printf("dispatch_cmd: <<< %s, status %s (%s)\n",
				_gdp_proto_cmd_name(cmd), _gdp_proto_cmd_name(req->pkt->cmd),
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(Dbg, 30))
			_gdp_pkt_dump(req->pkt, ep_dbg_getfile());
	}
	return estat;
}


/*
**	GDPD_REQ_THREAD --- per-request thread
**
**		Reads and processes a command from the network port.  When
**		it returns it gets put back in the thread pool.
*/

void
gdpd_req_thread(void *req_)
{
	gdp_req_t *req = req_;

	ep_dbg_cprintf(Dbg, 18, "gdpd_req_thread: starting\n");

	// find the GCL handle (if any)
//	req->gclh = _gdp_gcl_cache_get(req->pkt->gcl_name, 0);

	// got the packet, dispatch it based on the command
	req->stat = dispatch_cmd(req);
	//XXX anything to do with estat here?

	// see if we have any return data
	ep_dbg_cprintf(Dbg, 41, "gdpd_req_thread: sending %zd bytes\n",
			evbuffer_get_length(req->pkt->datum->dbuf));

	// send the response packet header
	req->stat = _gdp_pkt_out(req->pkt, bufferevent_get_output(req->chan));
	//XXX anything to do with estat here?

	// if there is any post-processing to do, invoke the callback
	if (req->cb != NULL && req->postproc)
	{
		(req->cb)(req);
		req->postproc = false;
		req->cb = NULL;
	}

	// we can now unlock and free resources
	if (!EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
		_gdp_req_free(req);

	ep_dbg_cprintf(Dbg, 19, "gdpd_req_thread: returning to pool\n");
}


/*
**  LEV_READ_CB --- handle reads on command sockets
**
**		The ctx argument is unused, but we pass it down anyway in case
**		we ever want to make use of it.
*/

void
lev_read_cb(gdp_chan_t *chan, void *ctx)
{
	EP_STAT estat;
	gdp_req_t *req;

	// allocate a request buffer
	estat = _gdp_req_new(0, NULL, chan, 0, &req);
	if (!EP_STAT_ISOK(estat))
	{
		gdp_log(estat, "lev_read_cb: cannot allocate request");
		return;
	}
	req->pkt->datum = gdp_datum_new();

	estat = _gdp_pkt_in(req->pkt, bufferevent_get_input(chan));
	if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
	{
		// we don't yet have the entire packet in memory
		_gdp_req_free(req);
		return;
	}

	ep_dbg_cprintf(Dbg, 10, "\n*** Processing command %d=%s from socket %d\n",
			req->pkt->cmd, _gdp_proto_cmd_name(req->pkt->cmd),
			bufferevent_getfd(chan));

	// cheat: pass channel in req structure
	req->chan = chan;
	thread_pool_job *new_job = thread_pool_job_new(&gdpd_req_thread,
									NULL, req);
	thread_pool_add_job(CpuJobThreadPool, new_job);
}

/*
**	LEV_EVENT_CB --- handle special events on socket
*/

void
lev_event_cb(gdp_chan_t *chan, short events, void *ctx)
{
	if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
	{
		EP_STAT estat = ep_stat_from_errno(errno);
		int err = EVUTIL_SOCKET_ERROR();

		gdp_log(estat, "error on command socket: %d (%s)",
				err, evutil_socket_error_to_string(err));
	}

	if (EP_UT_BITSET(BEV_EVENT_ERROR | BEV_EVENT_EOF, events))
		bufferevent_free(chan);
}

/*
**	ACCEPT_CB --- called when a new connection is accepted
*/

void
accept_cb(struct evconnlistener *lev,
		evutil_socket_t sockfd,
		struct sockaddr *sa,
		int salen,
		void *ctx)
{
	struct event_base *evbase = GdpIoEventBase;
	gdp_chan_t *chan;
	union sockaddr_xx saddr;
	socklen_t slen = sizeof saddr;

	evutil_make_socket_nonblocking(sockfd);
	chan = bufferevent_socket_new(evbase, sockfd,
					BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);

	if (chan == NULL)
	{
		gdp_log(ep_stat_from_errno(errno),
				"accept_cb: could not allocate bufferevent");
		return;
	}

	if (getpeername(sockfd, &saddr.sa, &slen) < 0)
	{
		gdp_log(ep_stat_from_errno(errno),
				"accept_cb: connection from unknown peer");
	}
	else if (ep_dbg_test(Dbg, 20))
	{
		char abuf[INET6_ADDRSTRLEN];

		switch (saddr.sa.sa_family)
		{
		case AF_INET:
			inet_ntop(saddr.sa.sa_family, &saddr.sin.sin_addr,
					abuf, sizeof abuf);
			break;
		case AF_INET6:
			inet_ntop(saddr.sa.sa_family, &saddr.sin6.sin6_addr,
					abuf, sizeof abuf);
			break;
		default:
			strcpy("<unknown>", abuf);
			break;
		}
		ep_dbg_printf("accept_cb: connection from %s\n", abuf);
	}

	bufferevent_setcb(chan, lev_read_cb, NULL, lev_event_cb, NULL);
	bufferevent_enable(chan, EV_READ | EV_WRITE);
}

/*
**	LISTENER_ERROR_CB --- called if there is an error when listening
*/

void
listener_error_cb(struct evconnlistener *lev, void *ctx)
{
	struct event_base *evbase = evconnlistener_get_base(lev);
	int err = EVUTIL_SOCKET_ERROR();
	EP_STAT estat;

	estat = ep_stat_from_errno(errno);
	gdp_log(estat, "listener error %d (%s)",
			err, evutil_socket_error_to_string(err));
	event_base_loopexit(evbase, NULL);
}


/*
**	GDP_INIT --- initialize this library
*/

static EP_STAT
init_error(const char *msg)
{
	EP_STAT estat = ep_stat_from_errno(errno);
	char nbuf[40];

	strerror_r(errno, nbuf, sizeof nbuf);
	gdp_log(estat, "gdpd_init: %s", msg);
	ep_app_error("gdpd_init: %s: %s", msg, nbuf);
	return estat;
}

static struct cmdfuncs	CmdFuncs[] =
{
	{ GDP_CMD_CREATE,		cmd_create		},
	{ GDP_CMD_OPEN_AO,		cmd_open_ao		},
	{ GDP_CMD_OPEN_RO,		cmd_open_ro		},
	{ GDP_CMD_CLOSE,		cmd_close		},
	{ GDP_CMD_READ,			cmd_read		},
	{ GDP_CMD_PUBLISH,		cmd_publish		},
	{ GDP_CMD_SUBSCRIBE,	cmd_subscribe	},
	{ GDP_CMD_MULTIREAD,	cmd_multiread	},
	{ 0,					NULL			}
};

struct event_base	*GdpListenerEventBase;
pthread_t			ListenerEventLoopThread;

EP_STAT
gdpd_init(int listenport)
{
	EP_STAT estat;
	struct evconnlistener *lev;
	extern EP_STAT _gdp_do_init_1(void);

	estat = _gdp_do_init_1();
	EP_STAT_CHECK(estat, goto fail0);

	// register the commands we implement
	_gdp_register_cmdfuncs(CmdFuncs);

	if (GdpListenerEventBase == NULL)
	{
		GdpListenerEventBase = event_base_new();
		if (GdpListenerEventBase == NULL)
		{
			estat = ep_stat_from_errno(errno);
			ep_app_error("gdpd_init: could not GdpListenerEventBase");
		}
	}

	// set up the incoming evconnlistener
	if (listenport <= 0)
		listenport = ep_adm_getintparam("swarm.gdpd.controlport",
										GDP_PORT_DEFAULT);
	{
		union sockaddr_xx saddr;

		saddr.sin.sin_family = AF_INET;
		saddr.sin.sin_addr.s_addr = INADDR_ANY;
		saddr.sin.sin_port = htons(listenport);
		lev = evconnlistener_new_bind(GdpListenerEventBase,
				accept_cb,
				NULL,		// context
				LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | LEV_OPT_THREADSAFE,
				-1,
				&saddr.sa,
				sizeof saddr.sin);
	}
	if (lev == NULL)
		estat = init_error("gdpd_init: could not create evconnlistener");
	else
		evconnlistener_set_error_cb(lev, listener_error_cb);
	EP_STAT_CHECK(estat, goto fail0);

	// success!
	ep_dbg_cprintf(Dbg, 1, "gdpd_init: listening on port %d\n", listenport);
	return estat;

fail0:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 1, "gdpd_init: failed with stat %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


#ifndef SIGINFO
# define SIGINFO	SIGUSR1
#endif

void
siginfo(int sig, short what, void *arg)
{
	gcl_showusage(stderr);
	ep_dumpfds(stderr);
}

int
main(int argc, char **argv)
{
	int opt;
	int listenport = -1;
	bool run_in_foreground = false;
	EP_STAT estat;
	long nworkers = sysconf(_SC_NPROCESSORS_ONLN);

	while ((opt = getopt(argc, argv, "D:Fn:P:")) > 0)
	{
		switch (opt)
		{
		case 'D':
			run_in_foreground = true;
			ep_dbg_set(optarg);
			break;

		case 'F':
			run_in_foreground = true;
			break;

		case 'n':
			nworkers = atoi(optarg);
			break;

		case 'P':
			listenport = atoi(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	// initialize GDP and the EVENT library
	estat = gdpd_init(listenport);
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100]; 

		ep_app_abort("Cannot initialize gdp library: %s",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	estat = gcl_physlog_init();
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_app_abort("Cannot initialize gcl physlog: %s",
		        ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	if (nworkers <= 0)
		nworkers = 1;
	CpuJobThreadPool = thread_pool_new(nworkers);
	thread_pool_init(CpuJobThreadPool);

	// add a debugging signal to print out some internal data structures
	event_add(evsignal_new(GdpListenerEventBase, SIGINFO, siginfo, NULL), NULL);

	// start the event threads
	_gdp_start_event_loop_thread(&ListenerEventLoopThread, GdpListenerEventBase,
								"listener");
	_gdp_start_event_loop_thread(&_GdpIoEventLoopThread, GdpIoEventBase,
								"I/O");

	// should never get here
	pthread_join(ListenerEventLoopThread, NULL);
	ep_app_abort("Fell out of event loop");
}
