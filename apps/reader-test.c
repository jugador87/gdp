/* vim: set ai sw=4 sts=4 ts=4 : */

#include <gdp/gdp.h>
#include <ep/ep_dbg.h>
#include <ep/ep_app.h>
#include <event2/buffer.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>

EP_STAT
do_read(gdp_gcl_t *gclh)
{
	EP_STAT estat;
	uint32_t recno = 1;
	gdp_datum_t *datum = gdp_datum_new();

	for (;;)
	{
		estat = gdp_gcl_read(gclh, recno, datum);
		EP_STAT_CHECK(estat, break);
		fprintf(stdout, " >>> ");
		gdp_datum_print(datum, stdout);
		recno++;

		// flush any left over data
		if (gdp_buf_reset(datum->dbuf) < 0)
			printf("*** WARNING: buffer reset failed: %s\n",
					strerror(errno));
	}
	return estat;
}


EP_STAT
do_subscribe(gdp_gcl_t *gclh)
{
	EP_STAT estat;

	if (EP_STAT_ISOK(estat))
		estat = gdp_gcl_subscribe(gclh, 1, -1, NULL, NULL);
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[200];

		ep_app_abort("Cannot subscribe: %s",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	for (;;)
	{
		gdp_event_t *gev = gdp_event_next(true);
		switch (gdp_event_gettype(gev))
		{
		  case GDP_EVENT_DATA:
			fprintf(stdout, " >>> ");
			gdp_datum_print(gdp_event_getdatum(gev), stdout);
			break;

		  default:
			fprintf(stderr, "Unknown event type %d\n", gdp_event_gettype(gev));
			sleep(1);
			break;
		}
		gdp_event_free(gev);
	}
	
	// should never get here
	return estat;
}


int
main(int argc, char **argv)
{
	gdp_gcl_t *gclh;
	EP_STAT estat;
	char buf[200];
	gcl_name_t gclname;
	char *gclpname;
	int opt;
	bool subscribe = false;

	while ((opt = getopt(argc, argv, "D:s")) > 0)
	{
		switch (opt)
		{
		  case 'D':
			ep_dbg_set(optarg);
			break;

		  case 's':
			subscribe = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc <= 0)
	{
		fprintf(stderr, "Usage: %s [-D dbgspec] [-s] <gcl_name>\n",
				ep_app_getprogname());
		exit(EX_USAGE);
	}

	estat = gdp_init();
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}


	gclpname = argv[0];
	fprintf(stdout, "Reading GCL %s\n", gclpname);

	estat = gdp_gcl_internal_name(gclpname, gclname);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_abort("illegal GCL name syntax:\n\t%s", gclpname);
		exit(1);
	}

	estat = gdp_gcl_open(gclname, GDP_MODE_RO, &gclh);
	if (!EP_STAT_ISOK(estat))
	{
		char sbuf[100];

		ep_app_error("Cannot open GCL: %s",
				ep_stat_tostr(estat, sbuf, sizeof sbuf));
		goto fail0;
	}

	if (subscribe)
		estat = do_subscribe(gclh);
	else
		estat = do_read(gclh);

fail0:
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
