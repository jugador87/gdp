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
#include <gdp/gdp.h>
#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_hash.h>
#include <ep/ep_dbg.h>
#include <ep/ep_pcvt.h>
#include <ep/ep_stat.h>
#include <ep/ep_xlate.h>
#include <event2/buffer.h>
#include <pthread.h>
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

#define DEF_URI_PREFIX	"/gdp/v1/gcl"

const char	*GclUriPrefix;		// prefix on all REST calls
EP_HASH		*OpenGclCache;		// cache of open GCLs

// used in SCGI callbacks to pass around state
struct sockstate
{
    gcl_handle_t	*gcl_handle;	// associated GCL handle
    struct event	*event;		// associated event
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

    ep_dbg_cprintf(Dbg, 10, "scgi_write => %d, dead = %d\n", i, dead);
    if (i == 0)
    {
	char obuf[40];

	ep_app_error("scgi_write (%s) failed: %s",
		ep_pcvt_str(obuf, sizeof obuf, sbuf), strerror(errno));
    }
    else if (dead)
    {
	ep_dbg_cprintf(Dbg, 1, "dead is set\n");
    }
    req->dead = NULL;
}


EP_STAT
gdp_failure(scgi_request *req, char *code, char *msg, char *fmt, ...)
{
    char buf[200];
    FILE *fp = ep_fopensmem(buf, sizeof buf, "w");
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
show_gcl(char *gclpname,
	scgi_request *req)
{
    gdp_failure(req, "418", "Not Implemented", "");
    return GDP_STAT_NOT_IMPLEMENTED;
}


EP_STAT
read_msg(char *gclpname, long msgno, scgi_request *req)
{
    EP_STAT estat;
    gcl_handle_t *gclh = NULL;
    gdp_msg_t msg;
    gcl_name_t gcliname;
    struct evbuffer *revb;

    // for printing below
    gdp_gcl_internal_name(gclpname, gcliname);

    estat = gdp_gcl_open(gcliname, GDP_MODE_RO, &gclh);
    if (!EP_STAT_ISOK(estat))
	goto fail0;

    revb = evbuffer_new();

    estat = gdp_gcl_read(gclh, msgno, &msg, revb);
    if (!EP_STAT_ISOK(estat))
	goto fail0;

    // package up the results and send them back
    {
	char rbuf[1024];

	// figure out the response header
	{
	    FILE *fp;

	    fp = ep_fopensmem(rbuf, sizeof rbuf, "w");
	    if (fp == NULL)
	    {
		ep_app_abort("Cannot open memory for GCL read response: %s",
			strerror(errno));
	    }
	    fprintf(fp, "HTTP/1.1 200 GCL Message\r\n"
			"Content-Type: application/json\r\n"
			"GDP-USC-Name: %s\r\n"
			"GDP-Message-Number: %ld\r\n",
			gclpname,
			msgno);
	    if (msg.ts.stamp.tv_sec != TT_NOTIME)
	    {
		fprintf(fp, "GDP-Commit-Timestamp: ");
		tt_print_interval(&msg.ts, fp, false);
		fprintf(fp, "\r\n");
	    }
	    fprintf(fp, "\r\n");		// end of header
	    fputc('\0', fp);
	    fclose(fp);
	}

	// finish up sending the data out --- the extra copy is annoying
	{
	    size_t rlen = strlen(rbuf);
	    size_t dlen = evbuffer_get_length(revb);
	    char obuf[1024];
	    char *obp = obuf;

	    if (rlen + dlen > sizeof obuf)
		obp = ep_mem_malloc(rlen + dlen);

	    if (obp == NULL)
		ep_app_abort("Cannot allocate memory for GCL read response: %s",
			strerror(errno));

	    memcpy(obp, rbuf, rlen);
	    evbuffer_remove(revb, obp + rlen, dlen);
	    scgi_send(req, obp, rlen + dlen);
	    if (obp != obuf)
		ep_mem_free(obp);
	}
    }

    // finished
    evbuffer_free(revb);
    gdp_gcl_close(gclh);
    return estat;

fail0:
    {
	char ebuf[200];

	gdp_failure(req, "404", "Cannot read GCL", "ss", 
		"GCL", gclpname,
		"reason", ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    if (gclh != NULL)
	gdp_gcl_close(gclh);
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
    char *gclpname;	    // name of the GCL of interest
    EP_STAT estat = EP_STAT_OK;

    if (ep_dbg_test(Dbg, 3))
    {
	ep_dbg_printf("Got connection on port %d from %s:\n",
		req->descriptor->port->port, req->remote_addr);
	ep_dbg_printf("    %s %s\n", scgi_method_name(req->request_method),
		req->request_uri);
    }

    // strip off leading "/gdp/v1/gcl/" prefix
    if (GclUriPrefix == NULL)
	GclUriPrefix = ep_adm_getstrparam("gdp.rest.prefix", DEF_URI_PREFIX);
    uri = req->request_uri;
    if (strncmp(uri, GclUriPrefix, strlen(GclUriPrefix)) != 0)
	goto error404;
    uri += strlen(GclUriPrefix);
    if (*uri == '/')
	uri++;

    // next component is the GCL id (name)
    gclpname = uri;
    uri = strchr(uri, '/');
    if (uri == NULL)
	uri = "";
    else if (*uri != '\0')
	*uri++ = '\0';

    ep_dbg_cprintf(Dbg, 3, "    gcl=%s, uri=%s\n", gclpname, uri);

    // XXX if no GCL name, should we print all GCLs?
    if (*gclpname == '\0')
    {
	if (req->request_method == SCGI_METHOD_POST)
	{
	    // create a new GCL
	    ep_dbg_cprintf(Dbg, 5, "=== Create new GCL\n");
	    gcl_handle_t *gclh;
	    EP_STAT estat = gdp_gcl_create(NULL, NULL, &gclh);

	    if (EP_STAT_ISOK(estat))
	    {
		char sbuf[SCGI_MAX_OUTBUF_SIZE];
		const gcl_name_t *nname;
		gcl_pname_t nbuf;

		// return the name of the GCL
		nname = gdp_gcl_getname(gclh);
		gdp_gcl_printable_name(*nname, nbuf);
		snprintf(sbuf, sizeof sbuf,
			"HTTP/1.1 201 GCL created\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"{\r\n"
			"    \"gcl_name\": \"%s\"\r\n"
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
	// have a bare GCL name
	if (req->request_method == SCGI_METHOD_GET)
	    estat = show_gcl(gclpname, req);
	else if (req->request_method == SCGI_METHOD_POST)
	{
	    // append value to GCL
	    gdp_msg_t msg;
	    struct sockstate *ss = req->descriptor->cbdata;

	    ep_dbg_cprintf(Dbg, 5, "=== Add value to GCL\n");

	    // if we don't have an open GCL, get one
	    estat = EP_STAT_OK;
	    if (ss->gcl_handle == NULL)
	    {
		gcl_name_t gcliname;

		gdp_gcl_internal_name(gclpname, gcliname);
		estat = gdp_gcl_open(gcliname, GDP_MODE_AO, &ss->gcl_handle);
	    }
	    if (EP_STAT_ISOK(estat))
	    {
		memset(&msg, 0, sizeof msg);
		msg.data = req->body;
		msg.len = req->scgi_content_length;
		estat = gdp_gcl_append(ss->gcl_handle, &msg);
	    }
	    if (!EP_STAT_ISOK(estat))
	    {
		char ebuf[200];

		gdp_failure(req, "420" "Cannot append to GCL", "ss",
			"GCL", gclpname,
			"error", ep_stat_tostr(estat, ebuf, sizeof ebuf));
		goto error404;
	    }

	    // success: send a response
	    {
		char rbuf[1000];
		FILE *fp;

		fp = ep_fopensmem(rbuf, sizeof rbuf, "w");
		if (fp == NULL)
		{
		    // well, maybe not so successful
		    estat = ep_stat_from_errno(errno);
		    goto error500;
		}
		fprintf(fp,
			"HTTP/1.1 200 Successfully appended\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"{\r\n"
			"    \"msgno\": \"%d\"",
			msg.msgno);
		if (msg.ts.stamp.tv_sec != TT_NOTIME)
		{
		    fprintf(fp,
			    ",\r\n"
			    "    \"timestamp\": ");
		    tt_print_interval(&msg.ts, fp, false);
		}
		fprintf(fp, "\r\n}");
		fputc('\0', fp);
		fclose(fp);
		scgi_send(req, rbuf, strlen(rbuf));
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
	estat = read_msg(gclpname, atol(uri), req);
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
    return estat;

 error500:
    estat = gdp_failure(req, "500", "Internal Server Error", "s",
	    "errno", strerror(errno));
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
    
    // if we have an associated GCL handle, close it
    if (ss->gcl_handle != NULL)
	gdp_gcl_close(ss->gcl_handle);

    ep_mem_free(ss);
}

int
main(int argc, char **argv, char **env)
{
    int opt;
    int listenport = -1;
    useconds_t poll_delay;
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
	    GclUriPrefix = optarg;
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
	EP_STAT estat = gdp_init(true);
	char ebuf[100];

	if (!EP_STAT_ISOK(estat))
	{
	    ep_app_abort("Cannot initialize gdp library: %s",
		    ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
    }

    // Initialize SCGI library
    //scgi_register_fd_callbacks(fd_newfd_cb, fd_freefd_cb);
    if (scgi_initialize(listenport))
    {
	ep_dbg_cprintf(Dbg, 1, "%s: listening for SCGI on port %d\n",
		ep_app_getprogname(), listenport);
    }
    else
    {
	ep_app_error("could not initialize SCGI port %d: %s",
		listenport, strerror(errno));
	return EX_OSERR;
    }

    // start looking for SCGI connections
    //	XXX This should really be done through the event library
    //	    rather than by polling.  To do this right there should
    //	    be a pool of worker threads that would have the SCGI
    //	    connection handed off to them.
    //	XXX May be able to cheat by changing the select() in
    //	    scgi_update_connections_port to wait.  It's OK if this
    //	    thread hangs since the other work happens in a different
    //	    thread.
    poll_delay = ep_adm_getintparam("gdp.rest.scgi.pollinterval", 100000);
    for (;;)
    {
	scgi_request *req = scgi_recv();
	int dead = 0;

	if (req == NULL)
	{
	    usleep(poll_delay);
	    continue;
	}
	req->dead = &dead;
	process_scgi_req(req, 0, NULL);
    }
}
