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
**	LOGD_SOCK_CLOSE_CB --- free resources when we lose a router connection
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
	ep_dbg_cprintf(Dbg, 69, "gdpd_reclaim_resources\n");
	gcl_reclaim_resources();
}


/*
**  Do shutdown at exit
*/

void
shutdown_req(gdp_req_t *req)
{
	if (ep_dbg_test(Dbg, 59))
	{
		ep_dbg_printf("shutdown_req: ");
		_gdp_req_dump(req, ep_dbg_getfile());
	}

	if (EP_UT_BITSET(GDP_REQ_SRV_SUBSCR, req->flags))
		sub_send_message_notification(req, NULL, GDP_NAK_S_LOSTSUB);
}

void
logd_shutdown(void)
{
	ep_dbg_cprintf(Dbg, 1, "\n\n*** Shutting down GCL cache ***\n");
	_gdp_gcl_cache_shutdown(&shutdown_req);

	ep_dbg_cprintf(Dbg, 1, "\n\n*** Withdrawing all advertisements ***\n");
	logd_advertise_all(GDP_CMD_WITHDRAW);
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
	_gdp_gcl_cache_dump(stderr);
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

	// arrange to clean up resources periodically
	{
		long gc_intvl = ep_adm_getlongparam("swarm.gdplogd.reclaim.interval",
								15L);
		struct timeval tv = { gc_intvl, 0 };
		struct event *evtimer = event_new(GdpIoEventBase, -1,
									EV_PERSIST, &gdpd_reclaim_resources, NULL);
		event_add(evtimer, &tv);
	}

	// initialize connection
	void _gdp_pdu_process(gdp_pdu_t *, gdp_chan_t *);
	phase = "open connection";
	_GdpChannel = NULL;
	estat = _gdp_chan_open(router_addr, _gdp_pdu_process, &_GdpChannel);
	EP_STAT_CHECK(estat, goto fail0);
	_GdpChannel->close_cb = &logd_sock_close_cb;
	_GdpChannel->advertise = &logd_advertise_all;

	// start the event loop
	phase = "start event loop";
	estat = _gdp_evloop_init();
	EP_STAT_CHECK(estat, goto fail0);

	// advertise all of our GCLs
	phase = "advertise GCLs";
	estat = logd_advertise_all(GDP_CMD_ADVERTISE);
	EP_STAT_CHECK(estat, goto fail0);

	// arrange for clean shutdown
	atexit(&logd_shutdown);

	// print our name as a reminder
	{
		gdp_pname_t pname;
		const char *progname;
		const char *myname = NULL;

		progname = ep_app_getprogname();
		if (progname != NULL)
		{
			char argname[100];

			snprintf(argname, sizeof argname, "swarm.%s.gdpname", progname);
			myname = ep_adm_getstrparam(argname, NULL);
		}

		if (myname == NULL)
			myname = gdp_printable_name(_GdpMyRoutingName, pname);

		fprintf(stdout, "My GDP routing name = %s\n", myname);
	}

	/*
	**  At this point we should be running
	*/

	pthread_join(_GdpIoEventLoopThread, NULL);

	// should never get here
	ep_app_fatal("Fell out of GdpIoEventLoopThread");

fail0:
	{
		char ebuf[100];

		ep_app_fatal("Cannot initialize %s:\n\t%s",
				phase,
		        ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}
