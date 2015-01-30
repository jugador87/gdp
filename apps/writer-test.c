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
**  WRITER-TEST --- writes records to a GCL
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


void
usage(void)
{
	fprintf(stderr, "Usage: %s [-D dbgspec] [-G gdpd_addr] [-L log_file] gcl_name\n",
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

	// collect command-line arguments
	while ((opt = getopt(argc, argv, "D:G:L:")) > 0)
	{
		switch (opt)
		{
		 case 'D':
			ep_dbg_set(optarg);
			break;

		 case 'G':
			gdpd_addr = optarg;
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

	// open a GCL with the provided name
	gdp_parse_name(argv[0], gcliname);
	estat = gdp_gcl_open(gcliname, GDP_MODE_AO, NULL, &gcl);
	EP_STAT_CHECK(estat, goto fail1);

	// dump the internal version of the GCL to facilitate testing
	gdp_gcl_print(gcl, stdout, 0, 0);

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
		estat = gdp_gcl_append(gcl, datum);
		EP_STAT_CHECK(estat, goto fail2);

		// print the return value (shows the record number assigned)
		gdp_datum_print(datum, stdout);
	}

	// OK, all done.  Free our resources and exit
	gdp_datum_free(datum);

fail2:
	// tell the GDP that we are done
	gdp_gcl_close(gcl);

fail1:

fail0:
	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
