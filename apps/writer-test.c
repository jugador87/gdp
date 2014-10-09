/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <gdp/gdp.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>

/*
**  WRITER-TEST --- writes records to a GCL
**
**		This reads the records one line at a time from standard input
**		and assumes they are text, but there is no text requirement
**		implied by the GDP.
*/


int
main(int argc, char **argv)
{
	gdp_gcl_t *gclh;
	gcl_name_t gcliname;
	int opt;
	EP_STAT estat;
	char *gdpd_addr = NULL;
	bool append = false;
	char *xname = NULL;
	char buf[200];
	bool show_usage = false;

	// collect command-line arguments
	while ((opt = getopt(argc, argv, "aD:G:")) > 0)
	{
		switch (opt)
		{
		 case 'a':
			append = true;
			break;

		 case 'D':
			ep_dbg_set(optarg);
			break;

		 case 'G':
			gdpd_addr = optarg;
			break;

		 default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	// name is optional for a new GCL; if omitted one will be created
	if (argc > 0)
	{
		xname = argv[0];
		argc--;
		argv++;
	}
	if (show_usage || argc != 0 || (append && xname == NULL))
	{
		fprintf(stderr, "Usage: %s [-D dbgspec] [-a] [<gcl_name>]\n"
				"  (name is required for -a)\n",
				ep_app_getprogname());
		exit(EX_USAGE);
	}

	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	sleep(1);

	if (xname == NULL)
	{
		// create a new GCL handle with a new name
		estat = gdp_gcl_create(NULL, &gclh);
	}
	else
	{
		// open or create a GCL with the provided name
		gdp_gcl_parse_name(xname, gcliname);
		if (append)
			estat = gdp_gcl_open(gcliname, GDP_MODE_AO, &gclh);
		else
			estat = gdp_gcl_create(gcliname, &gclh);
	}
	EP_STAT_CHECK(estat, goto fail0);

	// dump the internal version of the GCL to facilitate testing
	gdp_gcl_print(gclh, stdout, 0, 0);

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
		estat = gdp_gcl_publish(gclh, datum);
		EP_STAT_CHECK(estat, goto fail1);

		// print the return value (shows the record number assigned)
		gdp_datum_print(datum, stdout);
	}

	// OK, all done.  Free our resources and exit
	gdp_datum_free(datum);

fail1:
	// tell the GDP that we are done
	gdp_gcl_close(gclh);

fail0:
	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
