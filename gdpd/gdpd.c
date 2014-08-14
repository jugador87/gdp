/* vim: set ai sw=4 sts=4 ts=4 : */

#include "gdpd.h"
#include "gdpd_physlog.h"
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
	return GDP_STAT_FROM_NAK(GDP_NAK_C_BADREQ);
}

void
flush_input_data(gdp_req_t *req, char *where)
{
	int i = gdp_buf_getlength(req->pkt.dbuf);

	if (i > 0)
	{
		ep_dbg_cprintf(Dbg, 4,
				"flush_input_data: %s: flushing %d bytes of unexpected input\n",
				where, i);
		gdp_buf_reset(req->pkt.dbuf);
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


/*
**  GDP command implementations
**
**		Each of these takes a request and an I/O channel (socket) as
**		arguments.
**
**		These routines should set req->pkt.cmd to the "ACK" reply
**		code, which will be used if the command succeeds (i.e.,
**		returns EP_STAT_OK).  Otherwise the return status is decoded
**		to produce a NAK code.  A specific NAK code can be sent
**		using GDP_STAT_FROM_NAK(nak).
**
**		All routines are expected to consume all their input from
**		the channel and to write any output to the same channel.
**		They can consume any unexpected input using flush_input_data.
*/

EP_STAT
cmd_create(gdp_req_t *req, gdp_chan_t *chan)
{
	EP_STAT estat;
	gcl_handle_t *gclh;

	req->pkt.cmd = GDP_ACK_DATA_CREATED;

	// no input, so we can reset the buffer just to be safe
	flush_input_data(req, "cmd_create");

	// do the physical create
	estat = gcl_create(req->pkt.gcl_name, &gclh);
	EP_STAT_CHECK(estat, goto fail0);

	// cache the open GCL Handle for possible future use
	EP_ASSERT_INSIST(!gdp_gcl_name_is_zero(gclh->gcl_name));
	_gdp_gcl_cache_add(gclh, GDP_MODE_AO);

	// pass any creation info back to the caller
	// (none at this point)

fail0:
	return estat;
}

EP_STAT
cmd_open_xx(gdp_req_t *req, gdp_chan_t *chan, gdp_iomode_t iomode)
{
	EP_STAT estat;
	gcl_handle_t *gclh;

	req->pkt.cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_open_xx");

	// see if we already know about this GCL
	gclh = _gdp_gcl_cache_get(req->pkt.gcl_name, iomode);
	if (gclh != NULL)
	{
		if (ep_dbg_test(Dbg, 12))
		{
			gcl_pname_t pname;

			gdp_gcl_printable_name(req->pkt.gcl_name, pname);
			ep_dbg_printf("cmd_open_xx: using cached handle for %s\n", pname);
		}
		rewind(gclh->fp);		// make sure we can switch modes (read/write)
		return EP_STAT_OK;
	}

	// nope, I guess we better open it
	if (ep_dbg_test(Dbg, 12))
	{
		gcl_pname_t pname;

		gdp_gcl_printable_name(req->pkt.gcl_name, pname);
		ep_dbg_printf("cmd_open_xx: doing initial open for %s\n", pname);
	}
	estat = gcl_open(req->pkt.gcl_name, iomode, &gclh);
	if (EP_STAT_ISOK(estat))
		_gdp_gcl_cache_add(gclh, iomode);
	return estat;
}

EP_STAT
cmd_open_ao(gdp_req_t *req, gdp_chan_t *chan)
{
	return cmd_open_xx(req, chan, GDP_MODE_AO);
}

EP_STAT
cmd_open_ro(gdp_req_t *req, gdp_chan_t *chan)
{
	return cmd_open_xx(req, chan, GDP_MODE_RO);
}

EP_STAT
cmd_close(gdp_req_t *req, gdp_chan_t *chan)
{
	gcl_handle_t *gclh;

	req->pkt.cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_close");

	gclh = _gdp_gcl_cache_get(req->pkt.gcl_name, GDP_MODE_ANY);
	if (gclh == NULL)
	{
		return gdpd_gcl_error(req->pkt.gcl_name, "cmd_close: GCL not open",
							GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
	}
	return gcl_close(gclh);
}

EP_STAT
cmd_read(gdp_req_t *req, gdp_chan_t *chan)
{
	gcl_handle_t *gclh;
	gdp_msg_t msg;
	EP_STAT estat;

	req->pkt.cmd = GDP_ACK_DATA_CONTENT;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_read");

	gclh = _gdp_gcl_cache_get(req->pkt.gcl_name, GDP_MODE_RO);
	if (gclh == NULL)
	{
		return gdpd_gcl_error(req->pkt.gcl_name, "cmd_read: GCL not open",
							GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
	}

	gdp_buf_reset(req->pkt.dbuf);
	estat = gcl_read(gclh, req->pkt.recno, &msg, req->pkt.dbuf);
	EP_STAT_CHECK(estat, goto fail0);
	req->pkt.dlen = EP_STAT_TO_LONG(estat);
	req->pkt.ts = msg.ts;

fail0:
	if (EP_STAT_IS_SAME(estat, EP_STAT_END_OF_FILE))
		req->pkt.cmd = GDP_NAK_C_NOTFOUND;
	return estat;
}


EP_STAT
cmd_publish(gdp_req_t *req, gdp_chan_t *chan)
{
	gcl_handle_t *gclh;
	gdp_msg_t msg;
	EP_STAT estat;
	int i;

	req->pkt.cmd = GDP_ACK_DATA_CREATED;

	gclh = _gdp_gcl_cache_get(req->pkt.gcl_name, GDP_MODE_AO);
	if (gclh == NULL)
	{
		return gdpd_gcl_error(req->pkt.gcl_name, "cmd_publish: GCL not open",
							GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
	}

	// read in the data from the bufferevent
	i = evbuffer_remove_buffer(bufferevent_get_input(chan),
			req->pkt.dbuf, req->pkt.dlen);
	if (i < req->pkt.dlen)
	{
		return gdpd_gcl_error(req->pkt.gcl_name, "cmd_publish: short read",
							GDP_STAT_SHORTMSG, GDP_NAK_S_INTERNAL);
	}

	// create the message
	memset(&msg, 0, sizeof msg);
	msg.dbuf = req->pkt.dbuf;
	msg.recno = req->pkt.recno;
	memcpy(&msg.ts, &req->pkt.ts, sizeof msg.ts);

	estat = gcl_append(gclh, &msg);

	// fill in return information
	req->pkt.recno = msg.recno;
	return estat;
}


EP_STAT
cmd_subscribe(gdp_req_t *req, gdp_chan_t *chan)
{
	gcl_handle_t *gclh;

	req->pkt.cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_subscribe");

	gclh = _gdp_gcl_cache_get(req->pkt.gcl_name, GDP_MODE_RO);
	if (gclh == NULL)
	{
		return gdpd_gcl_error(req->pkt.gcl_name, "cmd_subscribe: GCL not open",
							GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
	}
	return implement_me("cmd_subscribe");
}


EP_STAT
cmd_not_implemented(gdp_req_t *req, gdp_chan_t *chan)
{
	//XXX print/log something here?

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_not_implemented");

	req->pkt.cmd = GDP_NAK_S_INTERNAL;
	_gdp_pkt_out_hard(&req->pkt, bufferevent_get_output(chan));
	return GDP_STAT_NOT_IMPLEMENTED;
}


/*
**	DISPATCH_CMD --- dispatch a command via the DispatchTable
**
**	Parameters:
**		req --- the request to be satisified
**		conn --- the connection (socket) handle
*/

EP_STAT
dispatch_cmd(gdp_req_t *req,
			gdp_chan_t *chan)
{
	EP_STAT estat;
	int cmd = req->pkt.cmd;

	ep_dbg_cprintf(Dbg, 18, "dispatch_cmd: >>> command %d\n", req->pkt.cmd);

	estat = _gdp_req_dispatch(req, chan);

	// decode return status as an ACK/NAK command
	if (!EP_STAT_ISOK(estat))
	{
		if (EP_STAT_REGISTRY(estat) == EP_REGISTRY_UCB &&
				EP_STAT_MODULE(estat) == GDP_MODULE &&
				EP_STAT_DETAIL(estat) >= GDP_ACK_MIN &&
				EP_STAT_DETAIL(estat) <= GDP_NAK_MAX)
		{
			req->pkt.cmd = EP_STAT_DETAIL(estat);
		}
		else
		{
			// not recognized
			req->pkt.cmd = GDP_NAK_S_INTERNAL;
		}
	}

	if (ep_dbg_test(Dbg, 18))
	{
		char ebuf[200];

		ep_dbg_printf("dispatch_cmd: <<< %d, status %d (%s)\n",
				cmd, req->pkt.cmd,
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(Dbg, 30))
			_gdp_pkt_dump_hdr(&req->pkt, ep_dbg_getfile());
	}
	return estat;
}


/*
**	GDP_REQ_THREAD --- per-request thread
**
**		Reads and processes a command from the network port.  When
**		it returns it gets put back in the thread pool.
*/

void
gdp_req_thread(void *continue_data)
{
	gdp_req_t *req = continue_data;
	gdp_chan_t *chan = req->udata;

	// find the GCL handle (if any)
	req->gclh = _gdp_gcl_cache_get(req->pkt.gcl_name, 0);

	// got the packet, dispatch it based on the command
	req->stat = dispatch_cmd(req, chan);
	//XXX anything to do with estat here?

#if 0
	// find the GCL handle, if any
	{
		gcl_handle_t *gclh;

		gclh = _gdp_gcl_cache_get(req->pkt.gcl_name, 0);
		if (gclh != NULL)
		{
			evb = req->pkt.dbuf;
			if (ep_dbg_test(Dbg, 40))
			{
				gcl_pname_t pname;

				gdp_gcl_printable_name(gclh->gcl_name, pname);
				ep_dbg_printf("gdp_req_thread: using evb %p from %s\n", evb,
				        pname);
			}
		}
	}
#endif

	// see if we have any return data
	req->pkt.dlen = evbuffer_get_length(req->pkt.dbuf);
	ep_dbg_cprintf(Dbg, 41, "gdp_req_thread: sending %d bytes\n",
			req->pkt.dlen);

	// make sure nothing sneaks in...
	//XXX I think BEV_OPT_DEFER_CALLBACKS does the trick
//	if (req->pkt.dlen > 0)
//		evbuffer_lock(req->pkt.dbuf);

	// send the response packet header
	req->stat = _gdp_pkt_out(&req->pkt, bufferevent_get_output(chan));
	//XXX anything to do with estat here?

	// copy any data
	if (req->pkt.dlen > 0)
	{
		int i;

		//XXX evbuffer_add_buffer_reference might be faster
		i = evbuffer_remove_buffer(req->pkt.dbuf,
				bufferevent_get_output(chan),
				req->pkt.dlen);
		if (i < req->pkt.dlen)
		{
			ep_dbg_cprintf(Dbg, 2,
					"gdp_req_thread: short read (wanted %d, got %d)\n",
					req->pkt.dlen, i);
			req->stat = GDP_STAT_SHORTMSG;
		}
	}

	// we can now unlock
//	if (req->pkt.dlen > 0)
//		evbuffer_unlock(evb);
}


static void
req_free_cb(void *req_)
{
	_gdp_req_free(req_);
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
	estat = _gdp_req_new(0, NULL, 0, &req);
	if (!EP_STAT_ISOK(estat))
	{
		gdp_log(estat, "lev_read_cb: cannot allocate request");
		return;
	}

	estat = _gdp_pkt_in(&req->pkt, bufferevent_get_input(chan));
	if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
	{
		// we don't yet have the entire packet in memory
		_gdp_req_free(req);
		return;
	}

	// cheat: pass channel in req structure
	req->udata = chan;
	thread_pool_job *new_job = thread_pool_job_new(&gdp_req_thread,
									&req_free_cb, req);
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
	gdp_chan_t *chan = bufferevent_socket_new(evbase, sockfd,
					BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
	union sockaddr_xx saddr;
	socklen_t slen = sizeof saddr;

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
	int eno = errno;
	EP_STAT estat = ep_stat_from_errno(eno);

	gdp_log(estat, "gdpd_init: %s", msg);
	ep_app_error("gdpd_init: %s: %s", msg, strerror(eno));
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
		listenport = ep_adm_getintparam("swarm.gdp.controlport",
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

int
main(int argc, char **argv)
{
	int opt;
	int listenport = -1;
	bool run_in_foreground = false;
	EP_STAT estat;

	while ((opt = getopt(argc, argv, "D:FP:")) > 0)
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

	long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	CpuJobThreadPool = thread_pool_new(ncpu);
	thread_pool_init(CpuJobThreadPool);

	// start the event threads
	_gdp_start_event_loop_thread(&ListenerEventLoopThread, GdpListenerEventBase,
								"listener");
	_gdp_start_event_loop_thread(&_GdpIoEventLoopThread, GdpIoEventBase,
								"I/O");

	// should never get here
	pthread_join(ListenerEventLoopThread, NULL);
	ep_app_abort("Fell out of event loop");
}
