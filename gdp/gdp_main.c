/* vim: set ai sw=4 sts=4 ts=4 :*/

#include "gdp.h"
#include "gdp_event.h"
#include "gdp_priv.h"

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_syslog.h>

#include <event2/buffer.h>
#include <event2/thread.h>

#include <errno.h>
#include <signal.h>
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
**  GDP_PDU_PROC_THREAD --- process PDU --- heavy part
**
**		This is the part of PDU processing that should be done in
**		a thread if the command is heavy weight.  As a rule of
**		thumb, commands are heavy, acks/naks are light.
**
**		On entry, the request will be locked.  If we are done with
**		it, we have to either free it or unlock it.
*/

static void
gdp_pdu_proc_thread(void *req_)
{
	gdp_req_t *req = req_;
	int cmd = req->pdu->cmd;
	bool pdu_is_command = GDP_CMD_IS_COMMAND(cmd);
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 40, "gdp_pdu_proc_thread >>> req=%p, iscmd=%d\n",
			req, pdu_is_command);

	//XXX dispatch command or ack/nak
	estat = _gdp_req_dispatch(req);

	if (pdu_is_command)
	{
		/*** We are processing a command ***/

		// swap source and destination address for commands (now response)
		{
			gdp_name_t temp;

			memcpy(temp, req->pdu->src, sizeof temp);
			memcpy(req->pdu->src, req->pdu->dst, sizeof req->pdu->src);
			memcpy(req->pdu->dst, temp, sizeof req->pdu->dst);
		}

		// decode return status as protocol status
		req->pdu->cmd = GDP_NAK_S_INTERNAL;		// default to panic code
		if (EP_STAT_REGISTRY(estat) == EP_REGISTRY_UCB &&
			EP_STAT_MODULE(estat) == GDP_MODULE)
		{
			int d = EP_STAT_DETAIL(estat);

			if (EP_STAT_ISOK(estat))
			{
				if (d >= 200 && d < (200 + GDP_ACK_MAX - GDP_ACK_MIN))
					req->pdu->cmd = d - 200 + GDP_ACK_MIN;
				else
					req->pdu->cmd = GDP_ACK_SUCCESS;
			}
			else if (EP_STAT_ISERROR(estat))
			{
				if (d >= 400 && d < (400 + GDP_NAK_C_MAX - GDP_NAK_C_MIN))
					req->pdu->cmd = d - 400 + GDP_NAK_C_MIN;
//				else
//					req->pdu->cmd = GDP_ACK_C_BADREQ;
			}
			else if (EP_STAT_ISSEVERE(estat))
			{
				if (d >= 500 && d < (500 + GDP_NAK_S_MAX - GDP_NAK_S_MIN))
					req->pdu->cmd = d - 500 + GDP_NAK_S_MIN;
			}
		}
		else if (EP_STAT_ISOK(estat))
			req->pdu->cmd = GDP_ACK_SUCCESS;
//		else if (EP_STAT_ISERROR(estat))
//			req->pdu->cmd = GDP_ACK_XXX;

		// send response packet if appropriate
		if (GDP_CMD_NEEDS_ACK(cmd))
		{
			ep_dbg_cprintf(Dbg, 41,
					"gdp_pdu_proc_thread: sending %zd bytes\n",
					evbuffer_get_length(req->pdu->datum->dbuf));
			req->stat = _gdp_pdu_out(req->pdu, req->chan);
			//XXX anything to do with estat here?
		}

		//XXX do command post processing
		if (req->postproc)
		{
			(req->postproc)(req);
			req->postproc = NULL;
		}
	}
	else
	{
		/*** We are processing a response (ack/nak) ***/

		// ASSERT(all data from chan has been consumed);

		if (EP_UT_BITSET(GDP_REQ_CLT_SUBSCR, req->flags))
		{
			// send the status as an event
			estat = _gdp_subscr_event(req);
		}
		else
		{
			// return our status via the request
			req->stat = estat;
			req->flags |= GDP_REQ_DONE;
			if (ep_dbg_test(Dbg, 44))
			{
				ep_dbg_printf("gdp_pdu_proc_thread: signalling ");
				_gdp_req_dump(req, ep_dbg_getfile());
			}

			// wake up invoker, which will return the status
			ep_thr_cond_signal(&req->cond);
		}
	}

	// free up resources
	if (EP_UT_BITSET(GDP_REQ_CORE, req->flags) &&
		!EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
	{
		// _gdp_req_free also unlockes
		_gdp_req_free(req);
	}
	else
	{
		_gdp_req_unlock(req);
	}

	ep_dbg_cprintf(Dbg, 40, "gdp_pdu_proc_thread <<< done\n");
}


/*
**  _GDP_PDU_PROCESS --- process a PDU
**
**		This is responsible for the lightweight stuff that can happen
**		in the I/O thread, such as matching an ack/nak PDU with the
**		corresponding req.  It should never block.  The heavy lifting
**		is done in the routine above.
*/

void
_gdp_pdu_process(gdp_pdu_t *pdu, gdp_chan_t *chan)
{
	bool pdu_is_command = GDP_CMD_IS_COMMAND(pdu->cmd);
	gdp_gcl_t *gcl;
	gdp_req_t *req = NULL;
	EP_STAT estat;

	if (pdu_is_command)
	{
		// is a command: dst is the GCL
		gcl = _gdp_gcl_cache_get(pdu->dst, 0);
	}
	else
	{
		// is an ack/nak: src is the GCL
		gcl = _gdp_gcl_cache_get(pdu->src, 0);

		// find the corresponding request
		if (gcl != NULL)
		{
			req = _gdp_req_find(gcl, pdu->rid);
		}
		else if (ep_dbg_test(Dbg, 1))
		{
			gdp_pname_t pbuf;

			ep_dbg_printf("_gdp_pdu_process: GCL %s has no handle\n",
					gdp_printable_name(pdu->src, pbuf));
		}
	}

	if (req == NULL)
	{
		ep_dbg_cprintf(Dbg, 43,
				"_gdp_pdu_process: allocating new req for GCL %p\n", gcl);
		estat = _gdp_req_new(pdu->cmd, gcl, chan, pdu, GDP_REQ_CORE, &req);
		EP_STAT_CHECK(estat, goto fail0);
	}
	else if (ep_dbg_test(Dbg, 43))
	{
		ep_dbg_printf("_gdp_pdu_process: using existing ");
		_gdp_req_dump(req, ep_dbg_getfile());
	}

	// we want to re-use caller's datum for (e.g.) read commands
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

	//XXX cmd: dispatch in thread; ack: dispatch directly
	if (pdu_is_command)
		ep_thr_pool_run(&gdp_pdu_proc_thread, req);
	else
		gdp_pdu_proc_thread(req);

	return;

fail0:
	ep_log(estat, "_gdp_pdu_process: cannot allocate request; dropping PDU");
	_gdp_pdu_free(pdu);		//XXX ??? who owns the pdu?
}


/*
**	Base loop to be called for event-driven systems.
**	Their events should have already been added to the event base.
**
**		GdpIoEventLoopThread is also used by gdplogd, hence non-static.
*/

pthread_t		_GdpIoEventLoopThread;

static void
event_loop_timeout(int fd, short what, void *eli_)
{
	struct event_loop_info *eli = eli_;

	ep_dbg_cprintf(Dbg, 79, "%s event loop timeout\n", eli->where);
}

static void *
run_event_loop(void *eli_)
{
	struct event_loop_info *eli = eli_;
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
**  Arrange to call atexit(3) functions on SIGINT and SIGTERM
*/

static void
exit_on_signal(int sig)
{
	fprintf(stderr, "\nExiting on signal %d\n", sig);
	exit(sig);
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

	ep_dbg_cprintf(Dbg, 4, "_gdp_lib_init:\n");

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

	// we can now re-adjust debugging
	ep_dbg_setfile(NULL);

	// arrange to call atexit(3) functions on SIGTERM
	if (ep_adm_getboolparam("swarm.gdp.catch.sigint", true))
		(void) signal(SIGINT, exit_on_signal);
	if (ep_adm_getboolparam("swarm.gdp.catch.sigterm", true))
		(void) signal(SIGTERM, exit_on_signal);

	// register status strings
	_gdp_stat_init();

	if (progname != NULL)
	{
		char argname[100];
		const char *logfac;

		// figure out or generate our name
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

		// allow log facilities on a per-app basis
		snprintf(argname, sizeof argname, "swarm.%s.log.facility", progname);
		logfac = ep_adm_getstrparam(argname, NULL);
		if (logfac == NULL)
			logfac = ep_adm_getstrparam("swarm.gdp.log.facility", "local4");
		ep_log_init(progname, ep_syslog_fac_from_name(logfac), stderr, NULL);
	}

	if (!gdp_name_is_valid(_GdpMyRoutingName))
		_gdp_newname(_GdpMyRoutingName);

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
		struct event_config *ev_cfg = event_config_new();

		event_config_require_features(ev_cfg, 0);
		GdpIoEventBase = event_base_new_with_config(ev_cfg);
		if (GdpIoEventBase == NULL)
			estat = init_error("could not create event base", "gdp_lib_init");
		event_config_free(ev_cfg);
		EP_STAT_CHECK(estat, goto fail0);
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
