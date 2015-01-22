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
**  PROCESS_PDU --- process a PDU
**
**		Called from the channel processing after an entire PDU
**		is in memory (but essentially no further processing).
*/

void
process_pdu(gdp_pdu_t *pdu, gdp_chan_t *chan)
{
	EP_STAT estat;
	gdp_req_t *req;

	// package PDU up as a request
	estat = _gdp_req_new(pdu->cmd, NULL, chan, pdu, 0, &req);
	EP_STAT_CHECK(estat, goto fail0);

	if (ep_dbg_test(Dbg, 66))
	{
		ep_dbg_printf("process_pdu: ");
		_gdp_req_dump(req, stderr);
	}

	// send it off to the thread pool for the rest
	ep_thr_pool_run(&gdpd_req_thread, req);
	return;

fail0:
	ep_log(estat, "process_pdu: cannot allocate request");
}


/*
**	LOGD_SOCK_CLOSE_CB --- free resources when we lose a connection
*/

void
logd_sock_close_cb(gdp_chan_t *chan)
{
	// free any requests tied to this channel
	gdp_req_t *req = LIST_FIRST(&chan->reqs);

	while (req != NULL)
	{
		gdp_req_t *req2 = LIST_NEXT(req, chanlist);
		_gdp_req_free(req);
		req = req2;
	}
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
	char *phase;

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

	/*
	**  Do initialization.  This is very order-sensitive
	*/

	// initialize GDP and the EVENT library
	phase = "gdp library";
	estat = _gdp_lib_init();
	EP_STAT_CHECK(estat, goto fail0);

	// initialize physical logs
	phase = "gcl physlog";
	estat = gcl_physlog_init();
	EP_STAT_CHECK(estat, goto fail0);

	// initialize the protocol module
	phase = "gdplogd protocol module";
	estat = gdpd_proto_init();
	EP_STAT_CHECK(estat, goto fail0);

	// initialize the thread pool
	ep_thr_pool_init(nworkers, nworkers, 0);

	// add a debugging signal to print out some internal data structures
	event_add(evsignal_new(GdpIoEventBase, SIGINFO, siginfo, NULL), NULL);

	// start the event loop
	phase = "start event loop";
	estat = _gdp_evloop_init();
	EP_STAT_CHECK(estat, goto fail0);

	// initialize connection
	phase = "open connection";
	_GdpChannel = NULL;
	estat = _gdp_chan_open(router_addr, process_pdu,  &_GdpChannel);
	EP_STAT_CHECK(estat, goto fail0);
	_GdpChannel->close_cb = &logd_sock_close_cb;

	// arrange to clean up resources periodically
	{
		long gc_intvl = ep_adm_getlongparam("swarm.gdplogd.reclaim.interval",
								15L);
		struct timeval tv = { gc_intvl, 0 };
		struct event *evtimer = event_new(GdpIoEventBase, -1,
									EV_PERSIST, &gdpd_reclaim_resources, NULL);
		event_add(evtimer, &tv);
	}

	// advertise all of our GCLs
	logd_advertise_all();

	/*
	**  At this point we should be running
	*/

	pthread_join(_GdpIoEventLoopThread, NULL);

	// should never get here
	ep_app_abort("Fell out of GdpIoEventLoopThread");

fail0:
	{
		char ebuf[100];

		ep_app_abort("Cannot initialize %s:\n\t%s",
				phase,
		        ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}
