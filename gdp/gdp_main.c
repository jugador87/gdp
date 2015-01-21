/* vim: set ai sw=4 sts=4 ts=4 :*/

#include "gdp.h"
#include "gdp_event.h"
#include "gdp_priv.h"

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>

#include <event2/event.h>
#include <event2/thread.h>

#include <errno.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.main", "GDP initialization and main loop");


/*
**  GDP Initialization and main event loop.
*/



/*
**  INIT_ERROR --- issue error on initialization problem
*/

static EP_STAT
init_error(const char *datum, const char *where)
{
	EP_STAT estat = ep_stat_from_errno(errno);
	char nbuf[40];

	strerror_r(errno, nbuf, sizeof nbuf);
	ep_log(estat, "gdp_init: %s: %s", where, datum);
	ep_app_error("gdp_init: %s: %s: %s", where, datum, nbuf);
	return estat;
}


/*
**  _GDP_PDU_PROCESS --- execute the command in the PDU
**
**		This applies only to regular GDP applications.  For
**		example, gdplogd has a completely different PDU
**		processing module.
*/

void
_gdp_pdu_process(gdp_pdu_t *pdu, gdp_chan_t *chan)
{
	EP_STAT estat;
	gdp_gcl_t *gcl = NULL;
	gdp_req_t *req = NULL;

	// find the handle for the GCL
	// (since this is an app, the dst is us, and the src is the GCL)
	gcl = _gdp_gcl_cache_get(pdu->src, 0);
	if (gcl != NULL)
	{
		// find the request
		req = _gdp_req_find(gcl, pdu->rid);
	}
	else if (ep_dbg_test(Dbg, 1))
	{
		gdp_pname_t pbuf;

		gdp_printable_name(pdu->src, pbuf);
		ep_dbg_printf("_gdp_pdu_process: GCL %s has no handle\n", pbuf);
	}

	if (req == NULL)
	{
		ep_dbg_cprintf(Dbg, 43,
				"_gdp_pdu_process: allocating new req for gcl %p\n", gcl);
		estat = _gdp_req_new(pdu->cmd, gcl, chan, pdu, 0, &req);
		if (!EP_STAT_ISOK(estat))
		{
			ep_log(estat, "_gdp_pdu_process: cannot allocate request; dropping packet");

			// not much to do here other than ignore the input
			_gdp_pdu_free(pdu);
			return;
		}
		
		//XXX link request into GCL list??
	}
	else if (ep_dbg_test(Dbg, 43))
	{
		ep_dbg_printf("_gdp_pdu_process: using existing ");
		_gdp_req_dump(req, ep_dbg_getfile());
	}

	if (req->pdu != pdu)
	{
		if (req->pdu->datum != NULL)
		{
			if (ep_dbg_test(Dbg, 43))
			{
				ep_dbg_printf("_gdp_pdu_process: reusing old datum "
						"for req %p\n   ",
						req);
				gdp_datum_print(req->pdu->datum, ep_dbg_getfile());
			}

			// don't need the old dbuf
			gdp_buf_free(req->pdu->datum->dbuf);

			// copy the contents of the new message over the old
			memcpy(req->pdu->datum, pdu->datum, sizeof *req->pdu->datum);

			// we no longer need the new message
			pdu->datum->dbuf = NULL;
			gdp_datum_free(pdu->datum);

			// point the new packet at the old datum
			pdu->datum = req->pdu->datum;
			EP_ASSERT(pdu->datum->inuse);
		}

		// can now drop the old pdu and switch to the new one
		req->pdu->datum = NULL;
		_gdp_pdu_free(req->pdu);
		req->pdu = pdu;
		pdu = NULL;
	}

	// invoke the command-specific (or ack-specific) function
	estat = _gdp_req_dispatch(req);

	// ASSERT(all data from chan has been consumed);

	ep_thr_mutex_lock(&req->mutex);

	if (EP_UT_BITSET(GDP_REQ_SUBSCRIPTION, req->flags))
	{
		// link the request onto the event queue
		gdp_event_t *gev;
		int evtype;

		// for the moment we only understand data responses (for subscribe)
		switch (req->pdu->cmd)
		{
		  case GDP_ACK_CONTENT:
			evtype = GDP_EVENT_DATA;
			break;

		  case GDP_ACK_DELETED:
			// end of subscription
			evtype = GDP_EVENT_EOS;
			break;

		  default:
			ep_dbg_cprintf(Dbg, 3, "Got unexpected ack %d\n", req->pdu->cmd);
			estat = GDP_STAT_PROTOCOL_FAIL;
			goto fail1;
		}

		estat = gdp_event_new(&gev);
		EP_STAT_CHECK(estat, goto fail1);

		gev->type = evtype;
		gev->gcl = req->gcl;
		gev->datum = req->pdu->datum;
		gev->udata = req->udata;
		req->pdu->datum = NULL;			// avoid use after free

		if (req->cb.generic != NULL)
		{
			// caller wanted a callback
#ifdef RUN_CALLBACKS_IN_THREAD
			// ... to run in a separate thread ...
			ep_thr_pool_run(req->cb.generic, gev);
#else
			// ... to run in I/O event thread ...
			(*req->cb.generic)(gev);
#endif
		}
		else
		{
			// caller wanted events
			gdp_event_trigger(gev);
		}

		// the callback must call gdp_event_free(gev)
	}
	else
	{
		// return our status via the request
		req->stat = estat;
		req->flags |= GDP_REQ_DONE;
		if (ep_dbg_test(Dbg, 44))
		{
			ep_dbg_printf("_gdp_pdu_process: on return, ");
			_gdp_req_dump(req, ep_dbg_getfile());
		}

		// wake up invoker, which will return the status
		ep_thr_cond_signal(&req->cond);
	}
fail1:
	ep_thr_mutex_unlock(&req->mutex);
}


/*
**	Base loop to be called for event-driven systems.
**	Their events should have already been added to the event base.
**
**		GdpIoEventLoopThread is also used by gdplogd, hence non-static.
*/

pthread_t		_GdpIoEventLoopThread;

static void
event_loop_timeout(int fd, short what, void *ctx)
{
	struct event_loop_info *eli = ctx;

	ep_dbg_cprintf(Dbg, 79, "%s event loop timeout\n", eli->where);
}

static void *
run_event_loop(void *ctx)
{
	struct event_loop_info *eli = ctx;
	struct event_base *evb = eli->evb;
	long evdelay = ep_adm_getlongparam("swarm.gdp.event.loopdelay", 100000L);
	
	// keep the loop alive if EVLOOP_NO_EXIT_ON_EMPTY isn't available
	long ev_timeout = ep_adm_getlongparam("swarm.gdp.event.looptimeout", 30L);
	struct timeval tv = { ev_timeout, 0 };
	struct event *evtimer = event_new(evb, -1, EV_PERSIST,
			&event_loop_timeout, eli);
	event_add(evtimer, &tv);

	for (;;)
	{
		if (ep_dbg_test(Dbg, 20))
		{
			ep_dbg_printf("gdp_event_loop: starting up %s base loop\n",
					eli->where);
			event_base_dump_events(evb, ep_dbg_getfile());
		}

#ifdef EVLOOP_NO_EXIT_ON_EMPTY
		event_base_loop(evb, EVLOOP_NO_EXIT_ON_EMPTY);
#else
		event_base_loop(evb, 0);
#endif

		if (ep_dbg_test(Dbg, 1))
		{
			ep_dbg_printf("gdp_event_loop: %s event_base_loop returned\n",
					eli->where);
			if (event_base_got_break(evb))
				ep_dbg_printf(" ... as a result of loopbreak\n");
			if (event_base_got_exit(evb))
				ep_dbg_printf(" ... as a result of loopexit\n");
		}
		if (event_base_got_exit(evb))
		{
			// the GDP daemon went away
			break;
		}

		if (evdelay > 0)
			ep_time_nanosleep(evdelay * 1000LL);		// avoid CPU hogging
	}

	ep_log(GDP_STAT_DEAD_DAEMON, "lost channel to gdp");
	ep_app_abort("lost channel to gdp");
}

EP_STAT
_gdp_start_event_loop_thread(pthread_t *thr,
		struct event_base *evb,
		const char *where)
{
	struct event_loop_info *eli = ep_mem_malloc(sizeof *eli);

	eli->evb = evb;
	eli->where = where;
	if (pthread_create(thr, NULL, run_event_loop, eli) != 0)
		return init_error("cannot create event loop thread", where);
	else
		return EP_STAT_OK;
}


/*
**   Logging callback for event library (for debugging).
*/

static EP_DBG	EvlibDbg = EP_DBG_INIT("gdp.evlib", "GDP Eventlib");

static void
evlib_log_cb(int severity, const char *msg)
{
	char *sev;
	char *sevstrings[] = { "debug", "msg", "warn", "error" };

	if (severity < 0 || severity > 3)
		sev = "?";
	else
		sev = sevstrings[severity];
	ep_dbg_cprintf(EvlibDbg, ((4 - severity) * 20) + 2, "[%s] %s\n", sev, msg);
}


/*
**  Initialization, Part 1:
**		Initialize the various external libraries.
**		Set up the I/O event loop base.
**		Initialize the GCL cache.
**		Start the event loop.
*/

EP_STAT
_gdp_lib_init(void)
{
	EP_STAT estat = EP_STAT_OK;
	const char *progname;
	const char *myname = NULL;

	ep_dbg_cprintf(Dbg, 4, "gdp_lib_init:\n");

	if (ep_dbg_test(EvlibDbg, 80))
	{
		// according to the book...
		//event_enable_debug_logging(EVENT_DBG_ALL);
		// according to the code...
		event_enable_debug_mode();
	}

	// initialize the EP library
	ep_lib_init(EP_LIB_USEPTHREADS);
	ep_adm_readparams("gdp");
	progname = ep_app_getprogname();
	if (progname != NULL)
		ep_adm_readparams(progname);

	// register status strings
	_gdp_stat_init();

	// figure out or generate our name
	if (progname != NULL)
	{
		char argname[100];

		snprintf(argname, sizeof argname, "swarm.%s.gdpname", progname);
		myname = ep_adm_getstrparam(argname, NULL);
		if (myname != NULL)
		{
			gdp_pname_t pname;

			estat = gdp_parse_name(myname, _GdpMyRoutingName);
			ep_dbg_cprintf(Dbg, 19, "Setting my name:\n\t%s\n\t%s\n",
					myname, gdp_printable_name(_GdpMyRoutingName, pname));
			EP_STAT_CHECK(estat, myname = NULL);
		}
	}
	if (myname == NULL)
	{
		gdp_pname_t pname;

		// no name found in configuration
		_gdp_newname(_GdpMyRoutingName);
		ep_dbg_cprintf(Dbg, 19, "Made up my name: %s\n",
				gdp_printable_name(_GdpMyRoutingName, pname));
	}

	if (ep_dbg_test(Dbg, 1))
	{
		gdp_pname_t pname;

		ep_dbg_printf("My GDP routing name = %s\n",
				gdp_printable_name(_GdpMyRoutingName, pname));
	}

	// initialize the GCL cache.  In theory this "cannot fail"
	estat = _gdp_gcl_cache_init();
	EP_STAT_CHECK(estat, goto fail0);

	// tell the event library that we're using pthreads
	if (evthread_use_pthreads() < 0)
		return init_error("cannot use pthreads", "gdp_lib_init");
	if (ep_dbg_test(Dbg, 90))
	{
		evthread_enable_lock_debuging();
	}

	// use our debugging printer
	event_set_log_callback(evlib_log_cb);

	// set up the event base
	if (GdpIoEventBase == NULL)
	{
		// Initialize for I/O events
		{
			struct event_config *ev_cfg = event_config_new();

			event_config_require_features(ev_cfg, 0);
			GdpIoEventBase = event_base_new_with_config(ev_cfg);
			if (GdpIoEventBase == NULL)
				estat = init_error("could not create event base", "gdp_lib_init");
			event_config_free(ev_cfg);
			EP_STAT_CHECK(estat, goto fail0);
		}
	}

	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 4, "gdp_lib_init: %s\n",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

fail0:
	return estat;
}


EP_STAT
_gdp_evloop_init(void)
{
	EP_STAT estat;

	// create a thread to run the event loop
	estat = _gdp_start_event_loop_thread(&_GdpIoEventLoopThread,
										GdpIoEventBase, "I/O");
	return estat;
}
