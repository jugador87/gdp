/* vim: set ai sw=4 sts=4 ts=4 : */

#include "gdpd.h"
#include "gdpd_physlog.h"
#include "gdpd_pubsub.h"

#include <ep/ep_string.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>

#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>


/************************  PRIVATE	************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gdpd.main", "GDP Daemon");



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

	// got the packet, dispatch it based on the command
	req->stat = dispatch_cmd(req);
	//XXX anything to do with estat here?

	// see if we have any return data
	ep_dbg_cprintf(Dbg, 41, "gdpd_req_thread: sending %zd bytes\n",
			evbuffer_get_length(req->pkt->datum->dbuf));

	// send the response packet header
	req->stat = _gdp_pkt_out(req->pkt, req->chan);
	//XXX anything to do with estat here?

	// if there is any post-processing to do, invoke the callback
	if (req->cb.gdpd != NULL && req->postproc)
	{
		(req->cb.gdpd)(req);
		req->postproc = false;
		req->cb.gdpd = NULL;
	}

	// we can now unlock and free resources
	if (!EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
		_gdp_req_free(req);

	ep_dbg_cprintf(Dbg, 19, "gdpd_req_thread: returning to pool\n");
}


/*
**  CMDSOCK_READ_CB --- handle reads on command sockets
*/

void
cmdsock_read_cb(struct bufferevent *bev, void *ctx)
{
	EP_STAT estat;
	gdp_req_t *req;
	struct gdp_event_info *gei = ctx;

	// allocate a request buffer
	estat = _gdp_req_new(0, NULL, gei->chan, 0, &req);
	if (!EP_STAT_ISOK(estat))
	{
		ep_log(estat, "cmdsock_read_cb: cannot allocate request");
		return;
	}
	req->pkt->datum = gdp_datum_new();

	estat = _gdp_pkt_in(req->pkt, gei->chan);
	if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
	{
		// we don't yet have the entire packet in memory
		_gdp_req_free(req);
		return;
	}

	ep_dbg_cprintf(Dbg, 10, "\n*** Processing command %d=%s from socket %d\n",
			req->pkt->cmd, _gdp_proto_cmd_name(req->pkt->cmd),
			bufferevent_getfd(bev));

	ep_thr_pool_run(&gdpd_req_thread, req);
}


/*
**  CMDSOCK_CLOSE_CB --- called when a socket closes
*/

void
cmdsock_close_cb(struct event_base *eb,
		struct gdp_event_info *gei)
{
	ep_dbg_cprintf(Dbg, 10, "cmdsock_close_cb:\n");

	// do cleanup, release resources, etc.
}


/*
**	CMDSOCK_EVENT_CB --- handle special events on listener socket
*/

void
cmdsock_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	struct gdp_event_info *gei = ctx;

	_gdp_event_cb(bev, events, ctx);
	if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
	{
		EP_STAT estat = ep_stat_from_errno(errno);
		int err = EVUTIL_SOCKET_ERROR();

		ep_log(estat, "error on command socket: %d (%s)",
				err, evutil_socket_error_to_string(err));
	}

	if (EP_UT_BITSET(BEV_EVENT_ERROR | BEV_EVENT_EOF, events))
	{
		// free any requests tied to this channel
		gdp_chan_t *chan = gei->chan;
		gdp_req_t *req = LIST_FIRST(&chan->reqs);

		while (req != NULL)
		{
			gdp_req_t *req2 = LIST_NEXT(req, chanlist);
			_gdp_req_free(req);
			req = req2;
		}

		// free up the bufferevent
		EP_ASSERT(bev == gei->chan->bev);
		bufferevent_free(bev);
		gei->chan->bev = NULL;

		// free up the channel memory
		ep_mem_free(gei->chan);
		gei->chan = NULL;
	}
}


/*
**	LEV_ACCEPT_CB --- called when a new connection is accepted
**
**		Called from evconnlistener with the new socket (which
**		turns into a control socket).  This is a bit weird
**		because the listener side has a different event base
**		than the control side.
*/

void
lev_accept_cb(struct evconnlistener *lev,
		evutil_socket_t sockfd,
		struct sockaddr *sa,
		int salen,
		void *ctx)
{
	struct event_base *evbase = GdpIoEventBase;
	gdp_chan_t *chan;
	union sockaddr_xx saddr;
	socklen_t slen = sizeof saddr;

	chan = ep_mem_zalloc(sizeof *chan);
	LIST_INIT(&chan->reqs);

	evutil_make_socket_nonblocking(sockfd);
	chan->bev = bufferevent_socket_new(evbase, sockfd,
					BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);

	if (chan->bev == NULL)
	{
		ep_log(ep_stat_from_errno(errno),
				"lev_accept_cb: could not allocate bufferevent");
		return;
	}

	if (getpeername(sockfd, &saddr.sa, &slen) < 0)
	{
		ep_log(ep_stat_from_errno(errno),
				"lev_accept_cb: connection from unknown peer");
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
		ep_dbg_printf("lev_accept_cb: connection from %s\n", abuf);
	}

	struct gdp_event_info *gei = ep_mem_zalloc(sizeof *gei);
	gei->exit_cb = &cmdsock_close_cb;
	gei->chan = chan;
	bufferevent_setcb(chan->bev, cmdsock_read_cb, NULL, cmdsock_event_cb, gei);
	bufferevent_enable(chan->bev, EV_READ | EV_WRITE);
}


/*
**	LEV_ERROR_CB --- called if there is an error when listening
*/

void
lev_error_cb(struct evconnlistener *lev, void *ctx)
{
	struct event_base *evbase = evconnlistener_get_base(lev);
	int err = EVUTIL_SOCKET_ERROR();
	EP_STAT estat;

	estat = ep_stat_from_errno(errno);
	ep_log(estat, "listener error %d (%s)",
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
	ep_log(estat, "gdpd_init: %s", msg);
	ep_app_error("gdpd_init: %s: %s", msg, nbuf);
	return estat;
}

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
	ep_adm_readparams("gdpd");

	// initialize the protocol module
	gdpd_proto_init();

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
				lev_accept_cb,
				NULL,		// context
				LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | LEV_OPT_THREADSAFE,
				-1,
				&saddr.sa,
				sizeof saddr.sin);
	}
	if (lev == NULL)
		estat = init_error("could not create evconnlistener");
	else
		evconnlistener_set_error_cb(lev, lev_error_cb);
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


/*
**  GDPD_RECLAIM_RESOURCES --- called periodically to prune old resources
**
**		This should really do reclaimation in a thread.
*/

static void
gdpd_reclaim_resources(int fd, short what, void *ctx)
{
	ep_dbg_cprintf(Dbg, 39, "reclaim_resources\n");
	gcl_reclaim_resources();
}


/*
**  SIGINFO --- called to print out internal state (for debugging)
**
**		On BSD and MacOS this is implemented as a SIGINFO (^T from
**		the command line), but since Linux doesn't have that we use
**		SIGUSR1 instead.
*/

#ifndef SIGINFO
# define SIGINFO	SIGUSR1
#endif

void
siginfo(int sig, short what, void *arg)
{
	gcl_showusage(stderr);
	ep_dumpfds(stderr);
}


/*
**  MAIN!
**
**		Sets the number of threads in the pool to twice the number
**		of cores.  It's not clear this is a good number.
**
**		XXX	Currently always runs in foreground.  This will change
**			to run in background unless -D or -F are specified.
**			Running in background should probably also turn off
**			SIGINFO, since it doesn't make sense in that context.
*/

int
main(int argc, char **argv)
{
	int opt;
	int listenport = -1;
	bool run_in_foreground = false;
	EP_STAT estat;
	int nworkers = -1;

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

		ep_app_abort("Cannot initialize gdp library:\n\t%s",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	estat = gcl_physlog_init();
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_app_abort("Cannot initialize gcl physlog:\n\t%s",
		        ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// initialize the thread pool
	ep_thr_pool_init(nworkers, nworkers, 0);

	// add a debugging signal to print out some internal data structures
	event_add(evsignal_new(GdpListenerEventBase, SIGINFO, siginfo, NULL), NULL);

	// arrange to clean up resources periodically
	{
		long gc_intvl = ep_adm_getlongparam("swarm.gdpd.reclaim.interval", 15L);
		struct timeval tv = { gc_intvl, 0 };
		struct event *evtimer = event_new(GdpListenerEventBase, -1,
									EV_PERSIST, &gdpd_reclaim_resources, NULL);
		event_add(evtimer, &tv);
	}

	// start the event threads
	_gdp_start_event_loop_thread(&ListenerEventLoopThread, GdpListenerEventBase,
								"listener");
	_gdp_start_event_loop_thread(&_GdpIoEventLoopThread, GdpIoEventBase,
								"I/O");

	// should never get here
	pthread_join(ListenerEventLoopThread, NULL);
	ep_app_abort("Fell out of ListenerEventLoopThread");
}
