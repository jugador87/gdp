/* vim: set ai sw=4 sts=4 ts=4 :*/
/*
**	RESTful interface to GDP
**
**		Seriously prototype.
**
**		Uses SCGI between the web server and this process.	We link
**		with Sam Alexander's SCGI C Library, http://www.xamuel.com/scgilib/
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_hash.h>
#include <ep/ep_dbg.h>
#include <ep/ep_pcvt.h>
#include <ep/ep_stat.h>
#include <ep/ep_xlate.h>
#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sysexits.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <sys/socket.h>
#include <scgilib/scgilib.h>
#include <event2/event.h>
#include <jansson.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.rest", "RESTful interface to GDP");

#define DEF_URI_PREFIX	"/gdp/v1"

const char		*GclUriPrefix;			// prefix on all REST calls
EP_HASH			*OpenGclCache;			// cache of open GCLs


/*
**  LOG_ERROR --- generic error logging routine
*/

void
log_error(const char *fmt, ...)
{
	va_list av;

	va_start(av, fmt);
	vfprintf(stderr, fmt, av);
	fprintf(stderr, ": %s\n", strerror(errno));
}

/*
**	SCGI_METHOD_NAME --- return printable name of method
**
**		Arguably this should be in SCGILIB.
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
	char xbuf[1024];
	char *xbase = xbuf;
	char *xp;
	char *sp;

	// first translate any lone newlines into crlfs (pity jansson won't do this)
	for (i = 0, sp = sbuf; (sp = strchr(sp, '\n')) != NULL; sp++)
		if (sp == sbuf || sp[-1] != '\r')
			i++;

	// i is now the count of newlines without carriage returns
	sp = sbuf;
	if (i > 0)
	{
		bool cr = false;

		// find the total number of bytes we need, using malloc if necessary
		i += strlen(sbuf) + 1;
		if (i > sizeof xbuf)
			xbase = ep_mem_malloc(i);
		xp = xbase;

		// xp now points to a large enough buffer, possibly malloced
		while (*sp != '\0')
		{
			if (*sp == '\n' && !cr)
				*xp++ = '\r';
			cr = *sp == '\r';
			*xp++ = *sp++;
		}
		*xp = '\0';

		sp = xbase;
	}


	// I don't quite understand what "dead" is all about.  It's copied
	// from the "helloworld" example.  Something about memory management,
	// but his example only seems to use it to print messages.
	req->dead = &dead;
	i = scgi_write(req, sp);

	// free buffer memory if necessary
	if (xbase != xbuf)
		ep_mem_free(xbase);

	ep_dbg_cprintf(Dbg, 10, "scgi_write => %d, dead = %d\n", i, dead);
	if (i == 0)
	{
		char obuf[40];
		char nbuf[40];

		strerror_r(errno, nbuf, sizeof nbuf);
		ep_app_error("scgi_write (%s) failed: %s",
				ep_pcvt_str(obuf, sizeof obuf, sbuf), nbuf);
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
	char buf[SCGI_MAX_OUTBUF_SIZE];
	va_list av;
	char c;
	json_t *j;
	char *jbuf;

	// set up the JSON object
	j = json_object();
	json_object_set_nocheck(j, "error", json_string(msg));
	json_object_set_nocheck(j, "code", json_string(code));
	json_object_set_nocheck(j, "uri", json_string(req->request_uri));
	json_object_set_nocheck(j, "method",
			json_string(scgi_method_name(req->request_method)));

	va_start(av, fmt);
	while ((c = *fmt++) != '\0')
	{
		char *key = va_arg(av, char *);

		switch (c)
		{
		case 's':
			json_object_set(j, key, json_string(va_arg(av, char *)));
			break;

		case 'd':
			json_object_set(j, key, json_integer((long) va_arg(av, int)));
			break;

		default:
			{
				char pbuf[40];

				snprintf(pbuf, sizeof pbuf, "Unknown format `%c'", c);
				json_object_set(j, key, json_string(pbuf));
			}
			break;
		}
	}
	va_end(av);

	// get it in string format
	jbuf = json_dumps(j, JSON_INDENT(4));

	// create the entire SCGI return message
	snprintf(buf, sizeof buf,
			"HTTP/1.1 %s %s\r\n"
			"Content-Type: application/json\r\n"
			"\r\n"
			"%s\r\n",
			code, msg, jbuf);
	write_scgi(req, buf);

	// clean up
	json_decref(j);
	free(jbuf);

	// should chose something more appropriate here
	return EP_STAT_ERROR;
}


/*
**  GDP_SCGI_RECV --- guarded version of scgi_recv().
**
**		The SCGI library isn't reentrant, so we have to avoid conflict
**		here.
*/

EP_THR_MUTEX	ScgiRecvMutex	EP_THR_MUTEX_INITIALIZER;

scgi_request *
gdp_scgi_recv(void)
{
	scgi_request *req;

	ep_thr_mutex_lock(&ScgiRecvMutex);
	req = scgi_recv();
	ep_thr_mutex_unlock(&ScgiRecvMutex);
	return req;
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
**  PARSE_QUERY --- break up the query into key/value pairs
*/

struct qkvpair
{
	char	*key;
	char	*val;
};

int
parse_query(char *qtext, struct qkvpair *qkvs, int nqkvs)
{
	int n = 0;

	for (n = 0; qtext != NULL && n < nqkvs; n++)
	{
		char *p;

		if (*qtext == '\0')
			break;

		// skip to the next kv pair separator
		qkvs[n].key = strsep(&qtext, "&;");

		// separate the key and the value (if any)
		p = strchr(qkvs[n].key, '=');
		if (p != NULL && *p != '\0')
			*p++ = '\0';
		qkvs[n].val = p;
	}

	return n;
}


/*
**  FIND_QUERY_KV --- find a key-value pair in query
*/

char *
find_query_kv(const char *key, struct qkvpair *qkvs)
{
	while (qkvs->key != NULL)
	{
		if (strcasecmp(key, qkvs->key) == 0)
			return qkvs->val;
		qkvs++;
	}
	return NULL;
}


/*
**  ERROR404 --- issue a 404 "Not Found" error
*/

EP_STAT
error404(scgi_request *req, const char *detail)
{
	return gdp_failure(req, "404", "Not Found", "s",
			"detail", detail);
}


/*
**  ERROR405 --- issue a 405 "Method Not Allowed" error
*/

EP_STAT
error405(scgi_request *req, const char *detail)
{
	return gdp_failure(req, "405", "Method Not Allowed", "s",
			"detail", detail);
}


/*
**  ERROR500 --- issue a 500 "Internal Server Error" error
*/

EP_STAT
error500(scgi_request *req, const char *detail, int eno)
{
	char nbuf[40];

	strerror_r(eno, nbuf, sizeof nbuf);
	(void) gdp_failure(req, "500", "Internal Server Error", "ss",
			"errno", nbuf,
			"detail", detail);
	return ep_stat_from_errno(eno);
}


/*
**  ERROR501 --- issue a 501 "Not Implemented" error
*/

EP_STAT
error501(scgi_request *req, const char *detail)
{
	return gdp_failure(req, "501", "Not Implemented", "s",
			"detail", detail);
}



/*
**  A_NEW_GCL --- create new GCL
*/

EP_STAT
a_new_gcl(scgi_request *req, const char *name)
{
	gdp_gcl_t *gclh;
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 5, "=== Create new GCL (%s)\n",
			name == NULL ? "NULL" : name);

	if (name != NULL)
	{
		gcl_name_t gclname;

		gdp_gcl_parse_name(name, gclname);
		estat = gdp_gcl_create(gclname, &gclh);
	}
	else
	{
		estat = gdp_gcl_create(NULL, &gclh);
	}
	if (EP_STAT_ISOK(estat))
	{
		char sbuf[SCGI_MAX_OUTBUF_SIZE];
		const gcl_name_t *nname;
		gcl_pname_t nbuf;
		json_t *j = json_object();
		char *jbuf;

		// return the name of the GCL
		nname = gdp_gcl_getname(gclh);
		gdp_gcl_printable_name(*nname, nbuf);
		json_object_set_nocheck(j, "gcl_name", json_string(nbuf));

		jbuf = json_dumps(j, JSON_INDENT(4));

		snprintf(sbuf, sizeof sbuf,
				"HTTP/1.1 201 GCL created\r\n"
				"Content-Type: application/json\r\n"
				"\r\n"
				"%s\r\n",
				jbuf);
		write_scgi(req, sbuf);

		// clean up
		json_decref(j);
		free(jbuf);
	}
	else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_CONFLICT))
	{
		estat = gdp_failure(req, "409", "Conflict", "s",
					"reason", "GCL already exists");
	}
	else
	{
		estat = error500(req, "cannot create GCL", errno);
	}
	return estat;
}


/*
**  A_SHOW_GCL --- show information about a GCL
*/

EP_STAT
a_show_gcl(scgi_request *req, gcl_name_t gcliname)
{
	return error501(req, "GCL status not implemented");
}


/*
**  A_PUBLISH --- publish datum to GCL
*/

EP_STAT
a_publish(scgi_request *req, gcl_name_t gcliname)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_datum_t *datum = gdp_datum_new();
	gdp_gcl_t *gcl = NULL;

	ep_dbg_cprintf(Dbg, 5, "=== Publish value to GCL\n");

	//XXX violates the principle that gdp-rest is "just an app"
	if ((gcl = _gdp_gcl_cache_get(gcliname, GDP_MODE_AO)) == NULL)
	{
		estat = gdp_gcl_open(gcliname, GDP_MODE_AO, &gcl);
	}
	if (EP_STAT_ISOK(estat))
	{
		gdp_datum_t *datum = gdp_datum_new();

		gdp_buf_write(datum->dbuf, req->body, req->scgi_content_length);
		estat = gdp_gcl_publish(gcl, datum);
	}
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[200];
		gcl_pname_t gclpname;

		gdp_gcl_printable_name(gcliname, gclpname);
		gdp_failure(req, "420" "Cannot append to GCL", "ss",
				"GCL", gclpname,
				"error", ep_stat_tostr(estat, ebuf, sizeof ebuf));
		return estat;
	}
	else
	{
		// success: send a response
		char rbuf[SCGI_MAX_OUTBUF_SIZE];
		json_t *j = json_object();
		char *jbuf;

		json_object_set_nocheck(j, "recno", json_integer(datum->recno));
		if (EP_TIME_ISVALID(&datum->ts))
		{
			char tbuf[100];

			ep_time_format(&datum->ts, tbuf, sizeof tbuf, false);
			json_object_set_nocheck(j, "timestamp", json_string(tbuf));
		}
		jbuf = json_dumps(j, JSON_INDENT(4));

		snprintf(rbuf, sizeof rbuf,
				"HTTP/1.1 200 Successfully appended\r\n"
				"Content-Type: application/json\r\n"
				"\r\n"
				"%s\r\n",
				jbuf);
		write_scgi(req, rbuf);

		// clean up
		free(jbuf);
		json_decref(j);
	}
	return estat;
}


/*
**	A_READ_DATUM --- read and return a datum from a GCL
**
**		XXX Currently doesn't use the GCL cache.  To make that work
**			long term we would have to have to implement LRU in that
**			cache (which we probably need to do anyway).
*/

EP_STAT
a_read_datum(scgi_request *req, gcl_name_t gcliname, gdp_recno_t recno)
{
	EP_STAT estat;
	gdp_gcl_t *gclh = NULL;
	gdp_datum_t *datum = gdp_datum_new();

	estat = gdp_gcl_open(gcliname, GDP_MODE_RO, &gclh);
	EP_STAT_CHECK(estat, goto fail0);

	estat = gdp_gcl_read(gclh, recno, datum);
	if (!EP_STAT_ISOK(estat))
		goto fail0;

	// package up the results and send them back
	{
		char rbuf[1024];

		// figure out the response header
		{
			FILE *fp;
			gcl_pname_t gclpname;

			fp = ep_fopensmem(rbuf, sizeof rbuf, "w");
			if (fp == NULL)
			{
				char nbuf[40];

				strerror_r(errno, nbuf, sizeof nbuf);
				ep_app_abort("Cannot open memory for GCL read response: %s",
						nbuf);
			}
			gdp_gcl_printable_name(gcliname, gclpname);
			fprintf(fp, "HTTP/1.1 200 GCL Message\r\n"
						"Content-Type: application/json\r\n"
						"GDP-GCL-Name: %s\r\n"
						"GDP-Record-Number: %" PRIgdp_recno "\r\n",
						gclpname,
						recno);
			if (EP_TIME_ISVALID(&datum->ts))
			{
				fprintf(fp, "GDP-Commit-Timestamp: ");
				ep_time_print(&datum->ts, fp, false);
				fprintf(fp, "\r\n");
			}
			fprintf(fp, "\r\n");				// end of header
			fputc('\0', fp);
			fclose(fp);
		}

		// finish up sending the data out --- the extra copy is annoying
		{
			size_t rlen = strlen(rbuf);
			size_t dlen = evbuffer_get_length(datum->dbuf);
			char obuf[1024];
			char *obp = obuf;

			if (rlen + dlen > sizeof obuf)
				obp = ep_mem_malloc(rlen + dlen);

			if (obp == NULL)
			{
				char nbuf[40];

				strerror_r(errno, nbuf, sizeof nbuf);
				ep_app_abort("Cannot allocate memory for GCL read response: %s",
						nbuf);
			}

			memcpy(obp, rbuf, rlen);
			gdp_buf_read(datum->dbuf, obp + rlen, dlen);
			scgi_send(req, obp, rlen + dlen);
			if (obp != obuf)
				ep_mem_free(obp);
		}
	}

	// finished
	gdp_datum_free(datum);
	gdp_gcl_close(gclh);
	return estat;

fail0:
	{
		char ebuf[200];
		gcl_pname_t gclpname;

		gdp_gcl_printable_name(gcliname, gclpname);
		gdp_failure(req, "404", "Cannot read GCL", "ss", 
				"GCL", gclpname,
				"reason", ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	if (gclh != NULL)
		gdp_gcl_close(gclh);
	return estat;
}


/*
**  GCL_DO_GET --- helper routine for GET method on a GCL
**
**		Have to look at query to figure out the semantics.
**		The query's the thing / wherein I'll catch the
**		conscience of the king.
*/

EP_STAT
gcl_do_get(scgi_request *req, gcl_name_t gcliname, struct qkvpair *qkvs)
{
	EP_STAT estat;
	char *qrecno = find_query_kv("recno", qkvs);
	char *qnrecs = find_query_kv("nrecs", qkvs);
	char *qtimeout = find_query_kv("timeout", qkvs);

	if (qnrecs != NULL)
	{
		// not yet implemented
		estat = error501(req, "nrecs query not supported");
	}
	else if (qtimeout != NULL)
	{
		// not yet implemented
		estat = error501(req, "timeout query not supported");
	}
	else if (qrecno != NULL)
	{
		gdp_recno_t recno = atol(qrecno);
		estat = a_read_datum(req, gcliname, recno);
	}
	else
	{
		estat = a_show_gcl(req, gcliname);
	}

	return estat;
}


/*
**  PFX_GCL --- process SCGI requests starting with /gdp/v1/gcl
*/

#define NQUERY_KVS		10		// max number of key-value pairs in query part

EP_STAT
pfx_gcl(scgi_request *req, char *uri)
{
	EP_STAT estat;
	struct qkvpair qkvs[NQUERY_KVS + 1];

	if (*uri == '/')
		uri++;

	ep_dbg_cprintf(Dbg, 3, "    gcl=%s\n    query=%s\n", uri, req->query_string);

	// parse the query, if it exists
	{
		int i = parse_query(req->query_string, qkvs, NQUERY_KVS);
		qkvs[i].key = qkvs[i].val = NULL;
	}

	if (*uri == '\0')
	{
		// no GCL name included
		switch (req->request_method)
		{
		case SCGI_METHOD_POST:
			// create a new GCL
			estat = a_new_gcl(req, NULL);
			break;

		case SCGI_METHOD_GET:
			// XXX if no GCL name, should we print all GCLs?
		default:
			// unknown URI/method
			estat = error405(req, "only GET or POST supported for unnamed GCL");
			break;
		}
	}
	else
	{
		gcl_name_t gcliname;

		// next component is the GCL id (name) in external format
		gdp_gcl_parse_name(uri, gcliname);

		// have a GCL name
		switch (req->request_method)
		{
		case SCGI_METHOD_GET:
			estat = gcl_do_get(req, gcliname, qkvs);
			break;

		case SCGI_METHOD_POST:
			// append value to GCL
			estat = a_publish(req, gcliname);
			break;

		case SCGI_METHOD_PUT:
			// create a new named GCL
			estat = a_new_gcl(req, uri);
			break;

		default:
			// unknown URI/method
			estat = error405(req, "only GET, POST, and PUT supported for named GCL");
		}
	}

	return estat;
}


/*
**  PFX_POST --- process SCGI requests starting with /gdp/v1/post
*/

EP_STAT
pfx_post(scgi_request *req, char *uri)
{
	if (*uri == '/')
		uri++;

	return error501(req, "/post/... URIs not yet supported");
}


/*
**	PROCESS_SCGI_REQ --- process an already-read SCGI request
**
**		This is generally called as an event
*/

EP_STAT
process_scgi_req(scgi_request *req)
{
	char *uri;				// the URI of the request
	EP_STAT estat = EP_STAT_OK;

	if (ep_dbg_test(Dbg, 3))
	{
		ep_dbg_printf("Got connection on port %d from %s:\n",
				req->descriptor->port->port, req->remote_addr);
		ep_dbg_printf("	   %s %s\n", scgi_method_name(req->request_method),
				req->request_uri);
	}

	// strip query string off of URI (I'm surprised it's not already done)
	uri = strchr(req->request_uri, '?');
	if (uri != NULL)
		*uri = '\0';

	// strip off leading "/gdp/v1/" prefix (error if not there)
	if (GclUriPrefix == NULL)
		GclUriPrefix = ep_adm_getstrparam("swarm.rest.prefix", DEF_URI_PREFIX);
	uri = req->request_uri;
	if (strncmp(uri, GclUriPrefix, strlen(GclUriPrefix)) != 0)
	{
		estat = error404(req, "improper URI prefix");
		goto finis;
	}
	uri += strlen(GclUriPrefix);
	if (*uri == '/')
		uri++;

	// next component is "gcl" for RESTful or "post" for RESTish
	if (strncmp(uri, "gcl", 3) == 0 && (uri[3] == '/' || uri[3] == '\0'))
	{
		// looking at "/gdp/v1/gcl/" prefix; next component is the GCL name
		estat = pfx_gcl(req, uri + 3);
	}
	else if (strncmp(uri, "post", 4) == 0 && (uri[4] == '/' || uri[4] == '\0'))
	{
		// looking at "/gdp/v1/post" prefix
		estat = pfx_post(req, uri + 4);
	}
	else
	{
		// looking at "/gdp/v1/<unknown>" prefix
		estat = error404(req, "unknown resource");
	}

finis:
	return estat;
}


int
main(int argc, char **argv, char **env)
{
	int opt;
	int listenport = -1;
	int64_t poll_delay;
	char *gdpd_addr = NULL;
	extern void run_scgi_protocol(void);

	while ((opt = getopt(argc, argv, "D:G:p:u:")) > 0)
	{
		switch (opt)
		{
		case 'D':						// turn on debugging
			ep_dbg_set(optarg);
			break;

		case 'G':						// gdp daemon host:port
			gdpd_addr = optarg;
			break;

		case 'p':						// select listen port
			listenport = atoi(optarg);
			break;

		case 'u':						// URI prefix
			GclUriPrefix = optarg;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (listenport < 0)
		listenport = ep_adm_getintparam("swarm.rest.scgiport", 8001);

	// Initialize the GDP library
	//		Also initializes the EVENT library and starts the I/O thread
	{
		EP_STAT estat = gdp_init(gdpd_addr);
		char ebuf[100];

		if (!EP_STAT_ISOK(estat))
		{
			ep_app_abort("Cannot initialize gdp library: %s",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
		}
	}

	// Initialize SCGI library
	if (scgi_initialize(listenport))
	{
		ep_dbg_cprintf(Dbg, 1, "%s: listening for SCGI on port %d\n",
				ep_app_getprogname(), listenport);
	}
	else
	{
		char nbuf[40];

		strerror_r(errno, nbuf, sizeof nbuf);
		ep_app_error("could not initialize SCGI port %d: %s",
				listenport, nbuf);
		return EX_OSERR;
	}

	// start looking for SCGI connections
	//	XXX This should really be done through the event library
	//		rather than by polling.	 To do this right there should
	//		be a pool of worker threads that would have the SCGI
	//		connection handed off to them.
	//	XXX May be able to cheat by changing the select() in
	//		scgi_update_connections_port to wait.  It's OK if this
	//		thread hangs since the other work happens in a different
	//		thread.
	poll_delay = ep_adm_getlongparam("swarm.gdp.rest.scgi.pollinterval", 100000);
	for (;;)
	{
		scgi_request *req = gdp_scgi_recv();
		int dead = 0;

		if (req == NULL)
		{
			(void) ep_time_nanosleep(poll_delay * 1000LL);
			continue;
		}
		req->dead = &dead;
		process_scgi_req(req);
	}
}
