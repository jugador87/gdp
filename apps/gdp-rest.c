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
//#include <jansson.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.rest", "RESTful interface to GDP");

#define DEF_URI_PREFIX	"/gdp/v1/gcl"

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

	// I don't quite understand what "dead" is all about.  It's copied
	// from the "helloworld" example.  Something about memory management,
	// but his example only seems to use it to print messages.
	req->dead = &dead;
	i = scgi_write(req, sbuf);

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
	char buf[4000];
	FILE *fp = ep_fopensmem(buf, sizeof buf, "w");
	va_list av;
	char c;

	EP_ASSERT(fp != NULL);

	fprintf(fp, "HTTP/1.1 %s\r\n"
				"Content-Type: application/json\r\n"
				"\r\n"
				"{\r\n"
				"	 \"error\": \"%s\"\r\n"
				"	 \"code\": \"%s\"\r\n",
				code, msg, code);
	va_start(av, fmt);
	while ((c = *fmt++) != '\0')
	{
		char *p = va_arg(av, char *);
		char pbuf[100];

		fprintf(fp, "	 \"%s\": \"", p);
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
	buf[sizeof buf - 1] = '\0';		// in case it overflowed
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


/*
**	READ_DATUM --- read and return a datum from a GCL
**
**		XXX Currently doesn't use the GCL cache.  To make that work
**			long term we would have to have to implement LRU in that
**			cache (which we probably need to do anyway).
*/

EP_STAT
read_datum(char *gclpname, gdp_recno_t recno, scgi_request *req)
{
	EP_STAT estat;
	gdp_gcl_t *gclh = NULL;
	gdp_datum_t *datum = gdp_datum_new();
	gcl_name_t gcliname;

	// for printing below
	estat = gdp_gcl_internal_name(gclpname, gcliname);
	EP_STAT_CHECK(estat, goto fail0);

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

			fp = ep_fopensmem(rbuf, sizeof rbuf, "w");
			if (fp == NULL)
			{
				char nbuf[40];

				strerror_r(errno, nbuf, sizeof nbuf);
				ep_app_abort("Cannot open memory for GCL read response: %s",
						nbuf);
			}
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
**	PROCESS_SCGI_REQ --- process an already-read SCGI request
**
**		This is generally called as an event
*/

EP_STAT
process_scgi_req(scgi_request *req)
{
	char *uri;				// the URI of the request
	char *gclpname;			// name of the GCL of interest
	EP_STAT estat = EP_STAT_OK;

	if (ep_dbg_test(Dbg, 3))
	{
		ep_dbg_printf("Got connection on port %d from %s:\n",
				req->descriptor->port->port, req->remote_addr);
		ep_dbg_printf("	   %s %s\n", scgi_method_name(req->request_method),
				req->request_uri);
	}

	// strip off leading "/gdp/v1/gcl/" prefix
	if (GclUriPrefix == NULL)
		GclUriPrefix = ep_adm_getstrparam("swarm.rest.prefix", DEF_URI_PREFIX);
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

	ep_dbg_cprintf(Dbg, 3, "	gcl=%s, uri=%s\n", gclpname, uri);

	// XXX if no GCL name, should we print all GCLs?
	if (*gclpname == '\0')
	{
		if (req->request_method == SCGI_METHOD_POST)
		{
			// create a new GCL
			ep_dbg_cprintf(Dbg, 5, "=== Create new GCL\n");
			gdp_gcl_t *gclh;
			EP_STAT estat = gdp_gcl_create(NULL, &gclh);

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
						"	 \"gcl_name\": \"%s\"\r\n"
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
			gdp_datum_t *datum = gdp_datum_new();
			gdp_gcl_t *gcl = NULL;

			ep_dbg_cprintf(Dbg, 5, "=== Add value to GCL\n");

			// if we don't have an open GCL, get one
			estat = EP_STAT_OK;
			if (gcl == NULL)
			{
				gcl_name_t gcliname;

				gdp_gcl_internal_name(gclpname, gcliname);
				//XXX violates the principle that gdp-rest is "just an app"
				if ((gcl = _gdp_gcl_cache_get(gcliname, GDP_MODE_AO)) == NULL)
				{
					estat = gdp_gcl_open(gcliname, GDP_MODE_AO, &gcl);
				}
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
						"	 \"recno\": \"%" PRIgdp_recno "\"",
						datum->recno);
				if (EP_TIME_ISVALID(&datum->ts))
				{
					fprintf(fp,
							",\r\n"
							"	 \"timestamp\": ");
					ep_time_print(&datum->ts, fp, false);
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
		estat = read_datum(gclpname, atol(uri), req);
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
	{
		char nbuf[40];

		strerror_r(errno, nbuf, sizeof nbuf);
		estat = gdp_failure(req, "500", "Internal Server Error", "s",
				"errno", nbuf);
	}
	return estat;
}


int
main(int argc, char **argv, char **env)
{
	int opt;
	int listenport = -1;
	int64_t poll_delay;
	extern void run_scgi_protocol(void);

	while ((opt = getopt(argc, argv, "D:p:u:")) > 0)
	{
		switch (opt)
		{
		case 'D':						// turn on debugging
			ep_dbg_set(optarg);
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
		EP_STAT estat = gdp_init();
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
