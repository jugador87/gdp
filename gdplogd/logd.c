/* vim: set ai sw=4 sts=4 ts=4 : */

#include "logd.h"
#include "logd_physlog.h"
#include "logd_pubsub.h"

#include <ep/ep_string.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>

#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.main", "GDP Log Daemon");



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
			evbuffer_get_length(req->pdu->datum->dbuf));

	// send the response packet header
	req->stat = _gdp_pdu_out(req->pdu, req->chan);
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
	req->pdu->datum = gdp_datum_new();

	estat = _gdp_pdu_in(req->pdu, gei->chan);
	if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
	{
		// we don't yet have the entire packet in memory
		_gdp_req_free(req);
		return;
	}

	ep_dbg_cprintf(Dbg, 10, "\n*** Processing command %d=%s from socket %d\n",
			req->pdu->cmd, _gdp_proto_cmd_name(req->pdu->cmd),
			bufferevent_getfd(bev));

	ep_thr_pool_run(&gdpd_req_thread, req);
}


/*
**	CMDSOCK_EXIT_CB --- free resources when we lose a connection
*/

void
cmdsock_exit_cb(struct bufferevent *bev, struct gdp_event_info *gei)
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
**	GDPLOGD_INIT --- initialize this service
*/

EP_STAT
gdplogd_init(const char *router_addr)
{
	EP_STAT estat;
	gdp_chan_t *chan;
	extern EP_STAT _gdp_do_init_1(void);
	extern EP_STAT _gdp_do_init_3(gdp_chan_t *);

	// step 1: set up global state
	estat = _gdp_do_init_1();
	EP_STAT_CHECK(estat, goto fail0);

	// step 2: initialize connection
	estat = _gdp_open_connection(router_addr, &chan);
	EP_STAT_CHECK(estat, goto fail0);
	_GdpChannel = chan;

	// step 3: start event loop
	struct gdp_event_info *gei = ep_mem_zalloc(sizeof *gei);
	gei->chan = chan;
	gei->exit_cb = &cmdsock_exit_cb;
	bufferevent_setcb(chan->bev, cmdsock_read_cb, NULL, _gdp_event_cb, gei);
	bufferevent_enable(chan->bev, EV_READ | EV_WRITE);
	estat = _gdp_start_event_loop_thread(&_GdpIoEventLoopThread,
										GdpIoEventBase, "I/O");
	EP_STAT_CHECK(estat, goto fail1);

	// step 4a: initialize the protocol module
	gdpd_proto_init();

	// step 4b: we will advertise ourselves later

	goto finis;

fail1:
	bufferevent_free(chan->bev);
	chan->bev = NULL;
	ep_mem_free(chan);

fail0:
finis:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 1, "gdplogd_init: %s\n",
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
**		XXX	Currently always runs in foreground.  This will change
**			to run in background unless -D or -F are specified.
**			Running in background should probably also turn off
**			SIGINFO, since it doesn't make sense in that context.
*/

int
main(int argc, char **argv)
{
	int opt;
	bool run_in_foreground = false;
	EP_STAT estat;
	int nworkers = -1;
	char *router_addr = NULL;

	while ((opt = getopt(argc, argv, "D:FG:n:")) > 0)
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

		case 'G':
			router_addr = optarg;
			break;

		case 'n':
			nworkers = atoi(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	// initialize GDP and the EVENT library
	estat = gdplogd_init(router_addr);
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
	event_add(evsignal_new(GdpIoEventBase, SIGINFO, siginfo, NULL), NULL);

	// advertise all of our GCLs
	logd_advertise_all();

	// arrange to clean up resources periodically
	{
		long gc_intvl = ep_adm_getlongparam("swarm.gdplogd.reclaim.interval",
								15L);
		struct timeval tv = { gc_intvl, 0 };
		struct event *evtimer = event_new(GdpIoEventBase, -1,
									EV_PERSIST, &gdpd_reclaim_resources, NULL);
		event_add(evtimer, &tv);
	}

	// should never get here
	pthread_join(_GdpIoEventLoopThread, NULL);
	ep_app_abort("Fell out of GdpIoEventLoopThread");
}
