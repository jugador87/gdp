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

/*
**  READER-TEST --- read and prints records from a GCL
**
**		This makes the naive assumption that all data values are ASCII
**		text.  Ultimately they should all be encrypted, but for now
**		I wanted to keep the code simple.
**
**		There are two ways of reading.  The first is to get individual
**		records in a loop, and the second is to request a batch of
**		records; these are returned as events that are collected after
**		the initial command completes.  There are two interfaces for
**		this; one only reads existing data, and the other will wait for
**		data to be published by another client.
*/


/*
**  DO_SIMPLEREAD --- read from a GCL using the one-record-at-a-time call
*/

EP_STAT
do_simpleread(gdp_gcl_t *gclh, gdp_recno_t firstrec, int numrecs)
{
	EP_STAT estat;
	gdp_recno_t recno = firstrec;
	gdp_datum_t *datum = gdp_datum_new();

	// make flags more user friendly
	if (recno <= 0)
		recno = 1;

	// change the "infinity" sentinel to make the loop easier
	if (numrecs == 0)
		numrecs = -1;

	// start reading data, one record at a time
	while (numrecs < 0 || --numrecs >= 0)
	{
		// ask the GDP to give us a record
		estat = gdp_gcl_read(gclh, recno, datum);

		// make sure it did; if not, break out of the loop
		EP_STAT_CHECK(estat, break);

		// print out the value returned
		fprintf(stdout, " >>> ");
		gdp_datum_print(datum, stdout);

		// move to the next record
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

	// end of data is returned as a "not found" error: turn it into a warning
	//    to avoid scaring the unsuspecting user
	if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
		estat = EP_STAT_END_OF_FILE;
	return estat;
}


/*
**  DO_MULTIREAD --- subscribe or multiread
**
**		This routine handles calls that return multiple values via the
**		event interface.  They might include subscriptions.
*/

EP_STAT
do_multiread(gdp_gcl_t *gclh, gdp_recno_t firstrec, int32_t numrecs, bool subscribe)
{
	EP_STAT estat;

	if (subscribe)
	{
		// start up a subscription
		estat = gdp_gcl_subscribe(gclh, firstrec, numrecs, NULL, NULL, NULL);
	}
	else
	{
		// make the flags more user-friendly
		if (firstrec == 0)
			firstrec = 1;

		// start up a multiread
		estat = gdp_gcl_multiread(gclh, firstrec, numrecs, NULL, NULL);
	}

	// check to make sure the subscribe/multiread succeeded; if not, bail
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[200];

		ep_app_abort("Cannot %s:\n\t%s",
				subscribe ? "subscribe" : "multiread",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// now start reading the events that will be generated
	for (;;)
	{
		// get the next incoming event
		gdp_event_t *gev = gdp_event_next(true);

		// decode it
		switch (gdp_event_gettype(gev))
		{
		  case GDP_EVENT_DATA:
			// this event contains a data return
			fprintf(stdout, " >>> ");
			gdp_datum_print(gdp_event_getdatum(gev), stdout);
			break;

		  case GDP_EVENT_EOS:
			// "end of subscription": no more data will be returned
			fprintf(stdout, "End of %s\n",
					subscribe ? "Subscription" : "Multiread");
			return estat;

		  default:
			// should be ignored, but we print it since this is a test program
			fprintf(stderr, "Unknown event type %d\n", gdp_event_gettype(gev));

			// just in case we get into some crazy loop.....
			sleep(1);
			break;
		}

		// don't forget to free the event!
		gdp_event_free(gev);
	}
	
	// should never get here
	return estat;
}


/*
**  MAIN --- the name says it all
*/

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
	bool multiread = false;
	int32_t numrecs = 0;
	gdp_recno_t firstrec = 0;

	// parse command-line options
	while ((opt = getopt(argc, argv, "D:f:mn:s")) > 0)
	{
		switch (opt)
		{
		  case 'D':
			// turn on debugging
			ep_dbg_set(optarg);
			break;

		  case 'f':
			// select the first record
			firstrec = atol(optarg);
			break;

		  case 'm':
			// turn on multi-read (see also -s)
			multiread = true;
			break;

		  case 'n':
			// select the number of records to be returned
			numrecs = atol(optarg);
			break;

		  case 's':
			// subscribe to this GCL (see also -m)
			subscribe = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	// we require a GCL name
	if (argc <= 0)
	{
		fprintf(stderr,
				"Usage: %s [-D dbgspec] [-f firstrec] [-m] [-n nrecs] [-s] <gcl_name>\n",
				ep_app_getprogname());
		exit(EX_USAGE);
	}

	// initialize the GDP library
	estat = gdp_init();
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	sleep(1);

	// parse the name (either base64-encoded or symbolic)
	estat = gdp_gcl_parse_name(argv[0], gclname);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_abort("illegal GCL name syntax:\n\t%s", argv[0]);
		exit(1);
	}

	// convert it to printable format and tell the user what we are doing
	gdp_gcl_printable_name(gclname, gclpname);
	fprintf(stdout, "Reading GCL %s\n", gclpname);

	// open the GCL; arguably this shouldn't be necessary
	estat = gdp_gcl_open(gclname, GDP_MODE_RO, &gclh);
	if (!EP_STAT_ISOK(estat))
	{
		char sbuf[100];

		ep_app_error("Cannot open GCL:\n    %s",
				ep_stat_tostr(estat, sbuf, sizeof sbuf));
		goto fail0;
	}

	// arrange to do the reading via one of the helper routines
	if (subscribe || multiread)
		estat = do_multiread(gclh, firstrec, numrecs, subscribe);
	else
		estat = do_simpleread(gclh, firstrec, numrecs);

	// might as well let the GDP know we're going away
	gdp_gcl_close(gclh);

fail0:
	// might as well let the user know what's going on....
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
