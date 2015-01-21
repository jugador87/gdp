/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  I/O CHANNEL HANDLING
**  	This communicates between the client and the routing layer.
*/

#include "gdp.h"
#include "gdp_priv.h"

#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>

#include <errno.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.proto", "GDP protocol processing");


/*
**	GDP_READ_CB --- data is available for reading from gdpd socket
**
**		Minimal implementation: read in packet and hand it to
**		processing routine.  If that processing is going to be
**		lengthy it should use a thread.
*/

static void
gdp_read_cb(struct bufferevent *bev, void *ctx)
{
	EP_STAT estat;
	gdp_pdu_t *pdu;
	gdp_buf_t *ievb = bufferevent_get_input(bev);
	gdp_chan_t *chan = ctx;

	ep_dbg_cprintf(Dbg, 50, "gdp_read_cb: fd %d, %zd bytes\n",
			bufferevent_getfd(bev), evbuffer_get_length(ievb));

	while (evbuffer_get_length(ievb) > 0)
	{
		pdu = _gdp_pdu_new();
		estat = _gdp_pdu_in(pdu, chan);
		if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
		{
			_gdp_pdu_free(pdu);
			return;
		}

		ep_dbg_cprintf(Dbg, 9,
				"\n*** Processing command %d=%s from socket %d\n",
				pdu->cmd, _gdp_proto_cmd_name(pdu->cmd),
				bufferevent_getfd(bev));

		(*chan->process)(pdu, chan);
	}
}


/*
**	GDP_EVENT_CB --- events or errors occur on gdpd socket
*/

static EP_PRFLAGS_DESC	EventWhatFlags[] =
{
	{ BEV_EVENT_READING,	BEV_EVENT_READING,		"BEV_EVENT_READING"		},
	{ BEV_EVENT_WRITING,	BEV_EVENT_WRITING,		"BEV_EVENT_WRITING"		},
	{ BEV_EVENT_EOF,		BEV_EVENT_EOF,			"BEV_EVENT_EOF"			},
	{ BEV_EVENT_ERROR,		BEV_EVENT_ERROR,		"BEV_EVENT_ERROR"		},
	{ BEV_EVENT_TIMEOUT,	BEV_EVENT_TIMEOUT,		"BEV_EVENT_TIMEOUT"		},
	{ BEV_EVENT_CONNECTED,	BEV_EVENT_CONNECTED,	"BEV_EVENT_CONNECTED"	},
	{ 0, 0, NULL }
};

static void
gdp_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	bool exitloop = false;
	gdp_chan_t *chan = ctx;

	if (ep_dbg_test(Dbg, 25))
	{
		ep_dbg_printf("_gdp_event_cb: ");
		ep_prflags(events, EventWhatFlags, ep_dbg_getfile());
		ep_dbg_printf(", fd=%d , errno=%d\n",
				bufferevent_getfd(bev), EVUTIL_SOCKET_ERROR());
	}
	if (EP_UT_BITSET(BEV_EVENT_CONNECTED, events))
	{
		chan->state = GDP_CHAN_CONNECTED;
		ep_thr_cond_broadcast(&chan->cond);
	}
	if (EP_UT_BITSET(BEV_EVENT_EOF, events))
	{
		gdp_buf_t *ievb = bufferevent_get_input(bev);
		size_t l = gdp_buf_getlength(ievb);

		ep_dbg_cprintf(Dbg, 1, "_gdp_event_cb: got EOF, %zu bytes left\n", l);
		exitloop = true;
	}
	if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
	{
		int sockerr = EVUTIL_SOCKET_ERROR();

		ep_dbg_cprintf(Dbg, 1, "_gdp_event_cb: error: %s\n",
				evutil_socket_error_to_string(sockerr));
		if (chan->state == GDP_CHAN_CONNECTING)
		{
			chan->state = GDP_CHAN_ERROR;
			ep_thr_cond_broadcast(&chan->cond);
		}
	}
	if (exitloop)
	{
		_gdp_chan_close(&chan);
		event_base_loopexit(bufferevent_get_base(bev), NULL);
	}
}


/*
**  _GDP_CHAN_OPEN --- open channel to the routing layer
*/

EP_STAT
_gdp_chan_open(const char *gdpd_addr,
		void (*process)(gdp_pdu_t *, gdp_chan_t *),
		gdp_chan_t **pchan)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_chan_t *chan;

	// allocate a new channel structure
	chan = ep_mem_zalloc(sizeof *chan);
	LIST_INIT(&chan->reqs);
	ep_thr_mutex_init(&chan->mutex, EP_THR_MUTEX_DEFAULT);
	ep_thr_cond_init(&chan->cond);
	chan->state = GDP_CHAN_CONNECTING;
	chan->process = process;

	// set up the bufferevent
	chan->bev = bufferevent_socket_new(GdpIoEventBase,
						-1,
						BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE |
						BEV_OPT_DEFER_CALLBACKS);
	if (chan->bev == NULL)
	{
		estat = ep_stat_from_errno(errno);
		ep_dbg_cprintf(Dbg, 18, "_gdp_chan_open: no bufferevent\n");
		goto fail0;
	}

	// have to do this early so event loop can find it
	*pchan = chan;

	// speak of the devil
	bufferevent_setcb(chan->bev, gdp_read_cb, NULL, gdp_event_cb, chan);
	bufferevent_enable(chan->bev, EV_READ | EV_WRITE);

	// attach it to a socket
	char abuf[500];
	char *port;
	char *host;

	// get the host:port info into abuf
	if (gdpd_addr == NULL)
		gdpd_addr = ep_adm_getstrparam("swarm.gdp.routers", "127.0.0.1");
	strlcpy(abuf, gdpd_addr, sizeof abuf);

	// strip off addresses and try them
	estat = GDP_STAT_NOTFOUND;				// anything that is not OK
	char *delim = abuf;
	do
	{
		char pbuf[10];

		host = delim;						// beginning of address spec
		delim = strchr(delim, ';');			// end of address spec
		if (delim != NULL)
			*delim++ = '\0';

		host = &host[strspn(host, " \t")];	// strip early spaces
		if (*host == '\0')
			continue;						// empty spec
		port = host;

		if (*host == '[')
		{
			// IPv6 literal
			host++;						// strip [] to satisfy getaddrinfo
			port = strchr(host, ']');
			if (port != NULL)
				*port++ = '\0';
		}

		// see if we have a port number
		if (port != NULL)
		{
			port = strchr(port, ':');
			if (port != NULL)
				*port++ = '\0';
		}
		if (port == NULL || *port == '\0')
		{
			port = pbuf;
			snprintf(pbuf, sizeof pbuf, "%d", GDP_PORT_DEFAULT);
		}

		ep_dbg_cprintf(Dbg, 20, "_gdp_chan_open: trying host %s port %s\n",
				host, port);

		// parsing done....  let's try the lookup
		struct addrinfo *res, *a;
		struct addrinfo hints;
		int r;

		memset(&hints, '\0', sizeof hints);
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		r = getaddrinfo(host, port, &hints, &res);
		if (r != 0)
		{
			// address resolution failed; try the next one
			continue;
		}

		// attempt connects on all available addresses
		ep_thr_mutex_lock(&chan->mutex);
		for (a = res; a != NULL; a = a->ai_next)
		{
			// must use a new socket each time to prevent errors
			{
				evutil_socket_t sock = bufferevent_getfd(chan->bev);
				if (sock >= 0)
				{
					close(sock);
					bufferevent_setfd(chan->bev, -1);
				}
			}

			// initiate the connect (will complete asynchronously)
			chan->state = GDP_CHAN_CONNECTING;
			if (bufferevent_socket_connect(chan->bev, a->ai_addr,
						a->ai_addrlen) < 0)
			{
				ep_dbg_cprintf(Dbg, 9,
						"_gdp_chan_open: bufferevent_socket_connect => %d\n",
						errno);
				continue;
			}

			// need to wait until the channel has had a chance
			while (chan->state == GDP_CHAN_CONNECTING)
			{
				ep_thr_cond_wait(&chan->cond, &chan->mutex, NULL);
			}
			if (chan->state == GDP_CHAN_CONNECTED)
			{
				// it was successful
				ep_dbg_cprintf(Dbg, 39, "successful connect\n");
				estat = EP_STAT_OK;
				break;
			}

			//XXX should do better testing of state here
		}
		ep_thr_mutex_unlock(&chan->mutex);
		freeaddrinfo(res);

		if (EP_STAT_ISOK(estat))
		{
			// success
			break;
		}
	} while (delim != NULL);

	// error cleanup and return
	if (!EP_STAT_ISOK(estat))
	{
		estat = ep_stat_from_errno(errno);
fail0:
		ep_log(estat, "_gdp_chan_open: could not create channel");
		if (chan != NULL)
		{
			if (chan->bev != NULL)
				bufferevent_free(chan->bev);
			chan->bev = NULL;
			ep_mem_free(chan);
		}
	}

	{
		char ebuf[80];

		ep_dbg_cprintf(Dbg, 20, "_gdp_open_channel => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  _GDP_CHAN_CLOSE --- close a channel (e.g., on error)
*/

void
_gdp_chan_close(gdp_chan_t **pchan)
{
	EP_ASSERT(pchan != NULL);
	gdp_chan_t *chan = *pchan;
	if (chan == NULL)
	{
		ep_dbg_cprintf(Dbg, 7, "_gdp_chan_close: null channel\n");
		return;
	}

	chan->state = GDP_CHAN_CLOSING;
	*pchan = NULL;
	if (chan->close_cb != NULL)
		(*chan->close_cb)(chan);
	bufferevent_free(chan->bev);
	chan->bev = NULL;
	ep_thr_cond_destroy(&chan->cond);
	ep_thr_mutex_destroy(&chan->mutex);
	ep_mem_free(chan);
}
