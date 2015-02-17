#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>

#include <jansson.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/time.h>

/*
**  MACHINE-MON --- monitor information on the hardware
**
**	This is just a sample program to demonstrate the interface.
**
**	This example is Linux-specific, but only because of the use
**	of the sysinfo(2) syscall.
**
**	To use this program:
**
**	(1) start up gdp_router on some appropriate machine.
**	(2) start up gdplogd on an appropriate machine (may be the
**	    same one) --- this presumes you have the GDP configured
**	    to find the routing node.
**	(3) start up this program on a program you wish to monitor,
**	    passing it the name of a log to hold the results.
**	(4) start up reader-test specifying a text subscription
**	    to the same log:
**	    	reader-test -s -t logname
**	(5) you should see the reader-test window spit out JSON
**	    records every 30 seconds showing a bunch of information
**	    about the machine being monitored.
**
**	The -h flag is because on the beaglebone the name is always
**	"beaglebone".
*/


gdp_gcl_t	*MonGcl;
unsigned int	SampleIntvl	= 60;


void
usage(void)
{
	fprintf(stderr,
		"Usage: %s [-D dbgspec] [-h name] [-s sec] logname\n"
		"    -D  turn on debugging\n"
		"    -h  hostname (logging tag)\n"
		"    -s  sample interval in seconds\n",
		ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	EP_STAT estat;
	gdp_name_t gclname;
	char *gclpname;
	bool show_usage = false;
	int opt;
	char *hostname = NULL;
	char hostbuf[100];

	while ((opt = getopt(argc, argv, "D:h:s:")) > 0)
	{
		switch (opt)
		{
		  case 'D':
			ep_dbg_set(optarg);
			break;

		  case 'h':
			hostname = optarg;
			break;

		  case 's':
			SampleIntvl = atol(optarg);
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

	gclpname = argv[0];

	if (hostname == NULL)
	{
		if (gethostname(hostbuf, sizeof hostbuf) < 0)
		{
			ep_app_abort("gethostname failed");
			exit(1);
		}
		hostname = hostbuf;
	}

	// initialize the GDP library
	printf("Initializing GDP library:\n");
	estat = gdp_init(NULL);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		exit(EX_UNAVAILABLE);
	}

	// convert the name to internal form
	printf("Converting name %s to internal form:\n", gclpname);
	estat = gdp_parse_name(gclpname, gclname);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("Illegal GCL name syntax:\n\t%s",
			gclpname);
		exit(EX_NOINPUT);
	}

	// open the GCL for writing
	printf("Opening GCL for writing:\n");
	estat = gdp_gcl_open(gclname, GDP_MODE_AO, NULL, &MonGcl);
	if (!EP_STAT_ISOK(estat))
	{
		char sbuf[100];

		ep_app_error("Cannot open GCL:\n    %s",
			ep_stat_tostr(estat, sbuf, sizeof sbuf));
		exit(EX_NOINPUT);
	}

	printf("Starting to write data:\n");

	for (;;)
	{
		struct sysinfo si;
		json_t *json;

		if (sysinfo(&si) < 0)
		{
			// how can sysinfo fail?
			ep_app_abort("Sysinfo failed: %s\n",
					strerror(errno));
		}

		// get the top-level option
		json = json_object();

		// fill it in with some potentially interesting information
		json_object_set(json, "host", json_string(hostname));
		json_object_set(json, "uptime", json_integer(si.uptime));
		json_object_set(json, "load1", json_integer(si.loads[0]));
		json_object_set(json, "load5", json_integer(si.loads[1]));
		json_object_set(json, "load15", json_integer(si.loads[2]));
		json_object_set(json, "freeram", json_integer(si.freeram));
		json_object_set(json, "nproc", json_integer(si.procs));

		// write it out... get a datum and the I/O buffer...
		gdp_datum_t *datum = gdp_datum_new();
		gdp_buf_t *buf = gdp_datum_getbuf(datum);

		// marshall the JSON to a string
		char *p = json_dumps(json, JSON_INDENT(4));

		// copy that string to the buffer
		gdp_buf_printf(buf, "%s", p);

		// append the datum to the log
		estat = gdp_gcl_append(MonGcl, datum);

		// remember to free our resources
		free(p);
		gdp_datum_free(datum);

		// sleep for a while
		sleep(SampleIntvl);
	}
	ep_app_abort("Impossible exit");
}
