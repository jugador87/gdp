/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_app.h>
#include <gdp/gdp.h>
#include <event2/buffer.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>

EP_STAT
do_read(gdp_gcl_t *gclh, gdp_recno_t firstrec)
{
	EP_STAT estat;
	gdp_recno_t recno = firstrec;
	gdp_datum_t *datum = gdp_datum_new();

	if (recno <= 0)
		recno = 1;
	for (;;)
	{
		estat = gdp_gcl_read(gclh, recno, datum);
		EP_STAT_CHECK(estat, break);
		fprintf(stdout, " >>> ");
		gdp_datum_print(datum, stdout);
		recno++;

		// flush any left over data
		if (gdp_buf_reset(gdp_datum_getbuf(datum)) < 0)
		{
			char nbuf[40];

			strerror_r(errno, nbuf, sizeof nbuf);
			printf("*** WARNING: buffer reset failed: %s\n",
					nbuf);
		}
	}

	// if we've reached the end of file, that's not an error, at least
	// as far as the user is concerned
	if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
		estat = EP_STAT_END_OF_FILE;
	return estat;
}


EP_STAT
do_subscribe(gdp_gcl_t *gclh, gdp_recno_t firstrec, int32_t numrecs)
{
	EP_STAT estat;

	estat = gdp_gcl_subscribe(gclh, firstrec, numrecs, NULL, NULL, NULL);
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

		  case GDP_EVENT_EOS:
			fprintf(stdout, "End of Subscription\n");
			return estat;

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
	gcl_pname_t gclpname;
	int opt;
	bool subscribe = false;
	int32_t numrecs = 0;
	gdp_recno_t firstrec = 1;

	while ((opt = getopt(argc, argv, "D:f:n:s")) > 0)
	{
		switch (opt)
		{
		  case 'D':
			ep_dbg_set(optarg);
			break;

		  case 'f':
			firstrec = atol(optarg);
			break;

		  case 'n':
			numrecs = atol(optarg);
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

	// allow thread to settle to avoid interspersed debug output
	sleep(1);

	estat = gdp_gcl_parse_name(argv[0], gclname);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_abort("illegal GCL name syntax:\n\t%s", argv[0]);
		exit(1);
	}

	gdp_gcl_printable_name(gclname, gclpname);
	fprintf(stdout, "Reading GCL %s\n", gclpname);

	estat = gdp_gcl_open(gclname, GDP_MODE_RO, &gclh);
	if (!EP_STAT_ISOK(estat))
	{
		char sbuf[100];

		ep_app_error("Cannot open GCL:\n    %s",
				ep_stat_tostr(estat, sbuf, sizeof sbuf));
		goto fail0;
	}

	if (subscribe)
		estat = do_subscribe(gclh, firstrec, numrecs);
	else
		estat = do_read(gclh, firstrec);

fail0:
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
