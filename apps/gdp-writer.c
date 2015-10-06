/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <gdp/gdp.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>
#include <sys/stat.h>

/*
**  GDP-WRITER --- writes records to a GCL
**
**		This reads the records one line at a time from standard input
**		and assumes they are text, but there is no text requirement
**		implied by the GDP.
*/


/*
**  DO_LOG --- log a timestamp (for performance checking).
*/

FILE	*LogFile;

void
do_log(const char *tag)
{
	struct timeval tv;

	if (LogFile == NULL)
		return;
	gettimeofday(&tv, NULL);
	fprintf(LogFile, "%s %ld.%06ld\n", tag, tv.tv_sec, (long) tv.tv_usec);
}

#define LOG(tag)	{ if (LogFile != NULL) do_log(tag); }


static char	*EventTypes[] =
{
	"Free (internal use)",
	"Data",
	"End of Subscription",
	"Shutdown",
	"Asynchronous Status",
};

void
showstat(gdp_event_t *gev)
{
	int evtype = gdp_event_gettype(gev);
	EP_STAT estat = gdp_event_getstat(gev);
	gdp_datum_t *d = gdp_event_getdatum(gev);
	char ebuf[100];
	char tbuf[20];
	char *evname;

	if (evtype < 0 || evtype >= sizeof EventTypes / sizeof EventTypes[0])
	{
		snprintf(tbuf, sizeof tbuf, "%d", evtype);
		evname = tbuf;
	}
	else
	{
		evname = EventTypes[evtype];
	}

	printf("Asynchronous event type %s:\n"
			"\trecno %" PRIgdp_recno ", stat %s\n",
			evname,
			gdp_datum_getrecno(d),
			ep_stat_tostr(estat, ebuf, sizeof ebuf));

	gdp_event_free(gev);
}


void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-a] [-D dbgspec] [-G router_addr] [-K key_file]\n"
			"\t[-L log_file] log_name\n"
			"    -a  use asynchronous I/O\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for gdp_router\n"
			"    -K  signing key file\n"
			"    -L  set logging file name (for debugging)\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	gdp_gcl_t *gcl;
	gdp_name_t gcliname;
	int opt;
	EP_STAT estat;
	char *gdpd_addr = NULL;
	char buf[200];
	bool show_usage = false;
	char *log_file_name = NULL;
	bool async_io = false;
	char *signing_key_file = NULL;
	gdp_gcl_open_info_t *info;

	// collect command-line arguments
	while ((opt = getopt(argc, argv, "aD:G:K:L:")) > 0)
	{
		switch (opt)
		{
		 case 'a':
			 async_io = true;
			 break;

		 case 'D':
			ep_dbg_set(optarg);
			break;

		 case 'G':
			gdpd_addr = optarg;
			break;

		 case 'K':
			signing_key_file = optarg;
			break;

		 case 'L':
			log_file_name = optarg;
			break;

		 default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc != 1)
		usage();

	if (log_file_name != NULL)
	{
		// open a log file (for timing measurements)
		LogFile = fopen(log_file_name, "a");
		if (LogFile == NULL)
			fprintf(stderr, "Cannot open log file %s: %s\n",
					log_file_name, strerror(errno));
		else
			setlinebuf(LogFile);
	}

	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	// set up any open information
	info = gdp_gcl_open_info_new();

	if (signing_key_file != NULL)
	{
		FILE *fp;
		EP_CRYPTO_KEY *skey;

		fp = fopen(signing_key_file, "r");
		if (fp == NULL)
		{
			ep_app_error("cannot open signing key file %s", signing_key_file);
			goto fail1;
		}

		skey = ep_crypto_key_read_fp(fp, signing_key_file,
				0, EP_CRYPTO_KEYFORM_PEM, EP_CRYPTO_F_SECRET);
		if (skey == NULL)
		{
			ep_app_error("cannot read signing key file %s", signing_key_file);
			goto fail1;
		}

		estat = gdp_gcl_open_info_set_signing_key(info, skey);
		EP_STAT_CHECK(estat, goto fail1);
	}

	// open a GCL with the provided name
	gdp_parse_name(argv[0], gcliname);
	estat = gdp_gcl_open(gcliname, GDP_MODE_AO, info, &gcl);
	EP_STAT_CHECK(estat, goto fail1);

	// dump the internal version of the GCL to facilitate testing
	{
		gdp_pname_t pname;

		printf("GDPname: %s (%" PRIu64 " recs)\n",
				gdp_printable_name(*gdp_gcl_getname(gcl), pname),
				gdp_gcl_getnrecs(gcl));
	}

	// OK, ready to go!
	fprintf(stdout, "\nStarting to read input\n");

	// we need a place to buffer the input
	gdp_datum_t *datum = gdp_datum_new();

	// start reading...
	while (fgets(buf, sizeof buf, stdin) != NULL)
	{
		// strip off newlines
		char *p = strchr(buf, '\n');
		if (p != NULL)
			*p++ = '\0';

		// echo the input for that warm fuzzy feeling
		fprintf(stdout, "Got input %s%s%s\n", EpChar->lquote, buf,
				EpChar->rquote);

		// send it to the GDP: first copy it into the buffer
		gdp_buf_write(gdp_datum_getbuf(datum), buf, strlen(buf));

		// then send the buffer to the GDP
		LOG("W");
		if (async_io)
		{
			estat = gdp_gcl_append_async(gcl, datum, showstat, NULL);
			EP_STAT_CHECK(estat, goto fail2);

			// return value will be printed asynchronously
		}
		else
		{
			estat = gdp_gcl_append(gcl, datum);
			EP_STAT_CHECK(estat, goto fail2);

			// print the return value (shows the record number assigned)
			gdp_datum_print(datum, stdout, 0);
		}
	}

	// OK, all done.  Free our resources and exit
	gdp_datum_free(datum);

fail2:
	// give a chance to collect async results
	if (async_io)
		sleep(1);

	// tell the GDP that we are done
	gdp_gcl_close(gcl);

fail1:
	if (info != NULL)
		gdp_gcl_open_info_free(info);

fail0:
	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
