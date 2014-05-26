/* vim: set ai sw=4 sts=4 :*/
/*
**  RESTful interface to GDP
**
**  	Seriously prototype.
**
**  	Uses SCGI between the web server and this process.  We link
**  	with Sam Alexander's SCGI C Library, http://www.xamuel.com/scgilib/
*/

#include <scgilib/scgilib.h>
#include <gdp/gdp_nexus.h>
#include <ep/ep.h>
#include <ep/ep_stat.h>
#include <ep/ep_dbg.h>
#include <ep/ep_pcvt.h>
#include <ep/ep_xlate.h>
#include <ep/ep_app.h>
#include <ep/ep_hash.h>
#include <unistd.h>
#include <stdio.h>
#include <sysexits.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <scgilib/scgilib.h>
#include <event2/event.h>
#include <jansson.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.rest", "RESTful interface to GDP");

char	*NexusUriPrefix = "/gdp/v1/nexus";  // prefix on all REST calls
EP_HASH	*OpenNexusCache;		    // cache of open nexuses

// used in SCGI callbacks to pass around state
struct sockstate
{
    nexdle_t	    *nexdle;	// associated nexdle
    struct event    *event;	// associated event
};

/*
**  SCGI_METHOD_NAME --- return printable name of method
**
**	Arguably this should be in SCGILIB.
*/

const char *
scgi_method_name(types_of_methods_for_http_protocol meth)
{
    switch (meth)
    {
	case SCGI_METHOD_UNSPECIFIED:
	    return "unspecified";
	case SCGI_METHOD_UNKNOWN:
	    return "unknown";
	case SCGI_METHOD_GET:
	    return "GET";
	case SCGI_METHOD_POST:
	    return "POST";
	case SCGI_METHOD_PUT:
	    return "PUT";
	case SCGI_METHOD_DELETE:
	    return "DELETE";
	case SCGI_METHOD_HEAD:
	    return "HEAD";
    }
    return "impossible";
}


void
write_scgi(scgi_request *req,
	char *sbuf)
{
    int dead = 0;
    int i;
    
    // I don't quite understand what "dead" is all about.  It's copied
    // from the "helloworld" example.  Something about memory management,
    // but his example only seems to use it to print messages.
    req->dead = &dead;
    i = scgi_write(req, sbuf);

    ep_dbg_cprintf(&Dbg, 10, "scgi_write => %d, dead = %d\n", i, dead);
    if (i == 0)
    {
	char obuf[40];

	fprintf(stderr, "scgi_write (%s) failed: %s\n",
		ep_pcvt_str(obuf, sizeof obuf, sbuf), strerror(errno));
    }
    else if (dead)
    {
	fprintf(stderr, "dead is set\n");
    }
    req->dead = NULL;
}


EP_STAT
gdp_failure(scgi_request *req, char *code, char *msg, char *fmt, ...)
{
    char buf[200];
    FILE *fp = ep_st_openmem(buf, sizeof buf, "w");
    va_list av;
    char c;

    EP_ASSERT(fp != NULL);

    fprintf(fp, "HTTP/1.1 %s\r\n"
		"Content-Type: application/json\r\n"
		"\r\n"
		"{\r\n"
		"    \"error\": \"%s\"\r\n", code, msg);
    fprintf(fp, "    \"code\": \"%s\"\r\n", code);
    va_start(av, fmt);
    while ((c = *fmt++) != '\0')
    {
	char *p = va_arg(av, char *);
	char pbuf[40];

	fprintf(fp, "    \"%s\": \"", p);
	switch (c)
	{
	case 's':
	    p = ep_pcvt_str(pbuf, sizeof pbuf, va_arg(av, char *));
	    break;

	case 'd':
	    p = ep_pcvt_int(pbuf, sizeof pbuf, (long) va_arg(av, int), 10);
	    break;

	default:
	    snprintf(pbuf, sizeof pbuf, "Unknown format `%c'", c);
	    p = pbuf;
	    break;
	}
	fprintf(fp, "%s\"\r\n", p);
    }
    va_end(av);
    fprintf(fp, "}");
    fputc('\0', fp);
    fclose(fp);
    write_scgi(req, buf);

    // should chose something more appropriate here
    return EP_STAT_ERROR;
}


EP_STAT
show_nexus(char *nexpname,
	scgi_request *req)
{
    gdp_failure(req, "418", "Not Implemented", "");
    return GDP_STAT_NOT_IMPLEMENTED;
}


const char *
format_timestamp(tt_interval_t *ts, char *tsbuf, size_t tsbufsiz)
{
    // for now just punt
    return "XXX Add Timestamp Here XXX";
}


EP_STAT
read_msg(char *nexpname, long msgno, scgi_request *req)
{
    EP_STAT estat;
    nexdle_t *nexdle = NULL;
    nexmsg_t msg;
    char mbuf[512];
    nname_t nexiname;

    // for printing below
    gdp_nexus_internal_name(nexpname, nexiname);

    estat = gdp_nexus_open(nexiname, GDP_MODE_RO, &nexdle);
    if (!EP_STAT_ISOK(estat))
	goto fail0;

    estat = gdp_nexus_read(nexdle, msgno, &msg, mbuf, sizeof mbuf);
    if (!EP_STAT_ISOK(estat))
	goto fail0;

    // package up the results and send them back
    {
	FILE *fp;
	char rbuf[1024];

	fp = ep_st_openmem(rbuf, sizeof rbuf, "w");
	if (fp == NULL)
	{
	    ep_app_abort("Cannot open memory for nexus read response: %s",
		    strerror(errno));
	}
	fprintf(fp, "HTTP/1.1 200 Nexus Message\r\n"
		    "Content-Type: application/json\r\n"
		    "\r\n"
		    "{\r\n"
		    "    \"nexus_name\": \"%s\"\r\n"
		    "    \"message_number\": \"%ld\"\r\n",
		    nexpname, msgno);
	if (msg.ts_valid)
	{
	    char tsbuf[200];

	    fprintf(fp, "    \"timestamp\": \"%s\"\r\n",
		    format_timestamp(&msg.ts, tsbuf, sizeof tsbuf));
	}
	fprintf(fp, "    \"value\": \"");
	ep_xlate_out(msg.data, msg.len, fp, "\"\r\n",
		EP_XLATE_PERCENT|EP_XLATE_8BIT|EP_XLATE_NPRINT);
	fprintf(fp, "\"\r\n"
		    "}\r\n");
	fclose(fp);
	scgi_write(req, rbuf);
    }

    // finished
    gdp_nexus_close(nexdle);
    return estat;

fail0:
    {
	char ebuf[200];

	gdp_failure(req, "404", "Cannot read nexus", "ss", 
		"nexus", nexpname,
		"reason", ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    if (nexdle != NULL)
	gdp_nexus_close(nexdle);
    return estat;
}


bool
is_integer_string(const char *s)
{
    do
    {
	if (!isdigit(*s))
	    return false;
    } while (*++s != '\0');
    return true;
}



/*
**  PROCESS_SCGI_REQ --- process an already-read SCGI request
**
**	This is generally called as an event
*/

EP_STAT
process_scgi_req(scgi_request *req,
		short what,
		void *arg)
{
    char *uri;		    // the URI of the request
    char *nexpname;	    // name of the nexus of interest
    EP_STAT estat = EP_STAT_OK;

    if (ep_dbg_test(&Dbg, 3))
    {
	ep_dbg_printf("Got connection on port %d from %s:\n",
		req->descriptor->port->port, req->remote_addr);
	ep_dbg_printf("    %s %s\n", scgi_method_name(req->request_method),
		req->request_uri);
    }

    // strip off leading "/gdp/v1/nexus/" prefix
    uri = req->request_uri;
    if (strncmp(uri, NexusUriPrefix, strlen(NexusUriPrefix)) != 0)
	goto error404;
    uri += strlen(NexusUriPrefix);
    if (*uri == '/')
	uri++;

    // next component is the nexus id (name)
    nexpname = uri;
    uri = strchr(uri, '/');
    if (uri == NULL)
	uri = "";
    else if (*uri != '\0')
	*uri++ = '\0';

    ep_dbg_cprintf(&Dbg, 3, "    nexus=%s, uri=%s\n", nexpname, uri);

    // XXX if no nexus name, should we print all nexuses?
    if (*nexpname == '\0')
    {
	if (req->request_method == SCGI_METHOD_POST)
	{
	    // create a new nexus
	    ep_dbg_cprintf(&Dbg, 5, "=== Create new nexus\n");
	    nexdle_t *nexdle;
	    EP_STAT estat = gdp_nexus_create(NULL, &nexdle);

	    if (EP_STAT_ISOK(estat))
	    {
		char sbuf[SCGI_MAX_OUTBUF_SIZE];
		const nname_t *nname;
		nexus_pname_t nbuf;

		// return the name of the nexus
		nname = gdp_nexus_getname(nexdle);
		gdp_nexus_printable_name(*nname, nbuf);
		snprintf(sbuf, sizeof sbuf,
			"HTTP/1.1 201 Nexus created\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"{\r\n"
			"    \"nexus_name\": \"%s\"\r\n"
			"}\r\n", nbuf);
		write_scgi(req, sbuf);
	    }
	}
	else
	{
	    // unknown URI/method
	    goto error404;
	}
    }
    else if (*uri == '\0')
    {
	// have a bare nexus name
	if (req->request_method == SCGI_METHOD_GET)
	    estat = show_nexus(nexpname, req);
	else if (req->request_method == SCGI_METHOD_POST)
	{
	    // append value to nexus
	    json_t *jroot;
	    json_t *jval;
	    json_error_t jerr;
	    char *etext = NULL;

	    ep_dbg_cprintf(&Dbg, 5, "=== Add value to nexus\n");

	    // first parse the JSON
	    jroot = json_loadb(req->body, req->scgi_content_length, 0, &jerr);
	    if (jroot == NULL)
		etext = jerr.text;
	    else if (!json_is_object(jroot))
		etext = "JSON root must be an object";
	    else if ((jval = json_object_get(jroot, "value")) == NULL ||
		    !json_is_string(jval))
		etext = "JSON must have a string value";
	    if (etext != NULL)
	    {
		gdp_failure(req, "400", "Illegal JSON Content", "s",
			"error_text", etext);
		estat = EP_STAT_ERROR;
		json_decref(jroot);
		goto fail1;
	    }

	    // now send it to the dataplane
	    {
		nexmsg_t msg;
		struct sockstate *ss = req->descriptor->cbdata;

		// if we don't have an open nexus, get one
		estat = EP_STAT_OK;
		if (ss->nexdle == NULL)
		{
		    nname_t nexiname;

		    gdp_nexus_internal_name(nexpname, nexiname);
		    estat = gdp_nexus_open(nexiname, GDP_MODE_AO, &ss->nexdle);
		}
		if (EP_STAT_ISOK(estat))
		{
		    memset(&msg, 0, sizeof msg);
		    msg.data = json_string_value(jval);
		    msg.len = strlen(msg.data);
		    estat = gdp_nexus_append(ss->nexdle, &msg);
		}
		if (!EP_STAT_ISOK(estat))
		{
		    char ebuf[200];

		    gdp_failure(req, "420" "Cannot append to nexus", "ss",
			    "nexus", nexpname,
			    "error", ep_stat_tostr(estat, ebuf, sizeof ebuf));
		    goto error404;
		}

		// success: send a response
		{
		    char rbuf[1000];

		    snprintf(rbuf, sizeof rbuf,
			    "HTTP/1.1 200 Successfully appended\r\n"
			    "Content-Type: application/json\r\n"
			    "\r\n"
			    "{\r\n"
			    "    \"msgno\": \"%ld\"\r\n"
			    "}\r\n",
			    msg.msgno);
		    scgi_send(req, rbuf, strlen(rbuf));
		}
	    }
	}
	else
	{
	    // unknown URI/method
	    goto error404;
	}
    }
    else if (req->request_method == SCGI_METHOD_GET && is_integer_string(uri))
    {
	// read message and return result
	estat = read_msg(nexpname, atol(uri), req);
    }
#if 0
    else if (req->request_method == SCGI_METHOD_POST)
    {
	// XXX
    }
#endif // 0
    else
    {
	// unrecognized URI
	goto error404;
    }

    return estat;

 error404:
    estat = gdp_failure(req, "404", "Unknown URI or method", "ss",
	    "uri", uri,
	    "method", scgi_method_name(req->request_method));

 fail1:
    return estat;
}

void
process_scgi_activity(evutil_socket_t fd, short what, void *arg)
{
    scgi_request *req;

    // This call will make the scgi library notice that something is happening.
    // But it could be something like a partial read that has to wait.
    req = scgi_recv();
    if (req != NULL)
	process_scgi_req(req, what, arg);
}

void *
fd_newfd_cb(int fd, enum scgi_fd_type fdtype)
{
    struct timeval tv = { 30, 0 };	// 30 second timeout
    struct sockstate *ss;

    if (fdtype == SCGI_FD_TYPE_ACCEPT)
    {
	int on = 1;

	// allow address reuse to make debugging easier
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
	setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof on);
    }

    ss = ep_mem_zalloc(sizeof *ss);

    // create an event
    ss->event = event_new(GdpEventBase, fd, EV_READ|EV_WRITE|EV_PERSIST,
	    process_scgi_activity, NULL);
    event_add(ss->event, &tv);

    return ss;
}

void
fd_freefd_cb(int fd, void *cbdata)
{
    struct sockstate *ss = cbdata;

    // clear the event so we can reuse the fd
    event_del(ss->event);
    event_free(ss->event);
    
    // if we have an associated nexdle, close it
    if (ss->nexdle != NULL)
	gdp_nexus_close(ss->nexdle);

    ep_mem_free(ss);
}

int
main(int argc, char **argv, char **env)
{
    int opt;
    int listenport = -1;
    extern void run_scgi_protocol(void);

    while ((opt = getopt(argc, argv, "D:p:u:")) > 0)
    {
	switch (opt)
	{
	case 'D':			// turn on debugging
	    ep_dbg_set(optarg);
	    break;

	case 'p':			// select listen port
	    listenport = atoi(optarg);
	    break;

	case 'u':			// URI prefix
	    NexusUriPrefix = optarg;
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (listenport < 0)
	listenport = ep_adm_getintparam("gdp.rest.scgiport", 8001);

    // Initialize the GDP library
    //	    Also initializes the EVENT library
    {
	EP_STAT estat = gdp_init();
	char ebuf[100];

	if (!EP_STAT_ISOK(estat))
	{
	    ep_app_abort("Cannot initialize gdp library: %s",
		    ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
    }

    // Initialize the Open Nexus Cache
    OpenNexusCache = ep_hash_new("Open Nexus Cache", NULL, 0);

    // Initialize SCGI library
    scgi_register_fd_callbacks(fd_newfd_cb, fd_freefd_cb);
    if (scgi_initialize(listenport))
    {
	ep_dbg_cprintf(&Dbg, 1, "%s: listening for SCGI on port %d\n",
		getprogname(), listenport);
    }
    else
    {
	ep_app_error("could not initialize SCGI port %d: %s",
		listenport, strerror(errno));
	return EX_OSERR;
    }

    //TODO: need to listen to other GDP events

    // run the event loop
#ifdef EVLOOP_NO_EXIT_ON_EMPTY
    event_base_loop(GdpEventBase, EVLOOP_NO_EXIT_ON_EMPTY);
#else
    for (;;)
    {
	long evdelay = ep_adm_getintparam("gdp.rest.event.loopdelay", 100000);

	event_base_loop(GdpEventBase, 0);
	if (evdelay > 0)
	    usleep(evdelay);			// avoid CPU hogging
    }
#endif

    // shouldn't return, but if it does....
    return EX_OK;
}
