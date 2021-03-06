/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-READER --- read and prints records from a GCL
**
**		This makes the naive assumption that all data values are ASCII
**		text.  Ultimately they should all be encrypted, but for now
**		I wanted to keep the code simple.
**
**		Unfortunately it isn't that simple, since it is possible to read
**		using all the internal mechanisms.  The -c, -m, and -s flags
**		control which approach is being used.
**
**		There are two ways of reading.  The first is to get individual
**		records in a loop (as implemented in do_simpleread), and the
**		second is to request a batch of records (as implemented in
**		do_multiread); these are returned as events that are collected
**		after the initial command completes or as callbacks that are
**		invoked in a separate thread.  There are two interfaces for the
**		event/callback techniques; one only reads existing data, and the
**		other ("subscriptions") will wait for data to be appended by
**		another client.
**
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**	----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_app.h>
#include <ep/ep_time.h>
#include <gdp/gdp.h>
#include <event2/buffer.h>

#include <unistd.h>
#include <errno.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp-reader", "GDP Reader Application");


/*
**  DO_LOG --- log a timestamp (for performance checking).
*/

FILE	*LogFile;
bool	TextData = false;		// set if data should be displayed as text
bool	PrintSig = false;		// set if signature should be printed
bool	Quiet = false;			// don't print metadata
int		NRead = 0;				// number of datums read

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

/*
**  PRINTDATUM --- just print out a datum
*/

void
printdatum(gdp_datum_t *datum, FILE *fp)
{
	uint32_t prflags = 0;

	if (TextData)
		prflags |= GDP_DATUM_PRTEXT;
	if (PrintSig)
		prflags |= GDP_DATUM_PRSIG;
	if (Quiet)
		prflags |= GDP_DATUM_PRQUIET;
	flockfile(fp);
	if (!Quiet)
		fprintf(fp, " >>> ");
	gdp_datum_print(datum, fp, prflags);
	funlockfile(fp);
	NRead++;
}


/*
**  DO_SIMPLEREAD --- read from a GCL using the one-record-at-a-time call
*/

EP_STAT
do_simpleread(gdp_gcl_t *gcl, gdp_recno_t firstrec, int numrecs)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_recno_t recno;
	gdp_datum_t *datum = gdp_datum_new();

	// change the "infinity" sentinel to make the loop easier
	if (numrecs == 0)
		numrecs = -1;

	// can't start reading before first record (but negative makes sense)
	if (firstrec == 0)
		firstrec = 1;

	// start reading data, one record at a time
	recno = firstrec;
	while (numrecs < 0 || --numrecs >= 0)
	{
		// ask the GDP to give us a record
		estat = gdp_gcl_read(gcl, recno, datum);
		LOG("R");

		// make sure it did; if not, break out of the loop
		EP_STAT_CHECK(estat, break);

		// print out the value returned
		printdatum(datum, stdout);

		// move to the next record
		recno++;

		// flush any left over data
		if (gdp_buf_reset(gdp_datum_getbuf(datum)) < 0)
		{
			char nbuf[40];

			strerror_r(errno, nbuf, sizeof nbuf);
			fprintf(stderr, "*** WARNING: buffer reset failed: %s\n",
					nbuf);
		}
	}

	// end of data is returned as a "not found" error: turn it into a warning
	//    to avoid scaring the unsuspecting user
	if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOTFOUND))
		estat = EP_STAT_END_OF_FILE;
	return estat;
}


EP_STAT
multiread_print_event(gdp_event_t *gev, bool subscribe)
{
	// decode it
	switch (gdp_event_gettype(gev))
	{
	  case GDP_EVENT_DATA:
		// this event contains a data return
		LOG("S");
		printdatum(gdp_event_getdatum(gev), stdout);
		break;

	  case GDP_EVENT_EOS:
		// "end of subscription": no more data will be returned
		fprintf(stderr, "End of %s\n",
				subscribe ? "Subscription" : "Multiread");
		return EP_STAT_END_OF_FILE;

	  case GDP_EVENT_SHUTDOWN:
		// log daemon has shut down, meaning we lose our subscription
		fprintf(stderr, "%s terminating because of log daemon shutdown\n",
				subscribe ? "Subscription" : "Multiread");
		return GDP_STAT_DEAD_DAEMON;

	  default:
		// should be ignored, but we print it since this is a test program
		fprintf(stderr, "Unknown event type %d\n", gdp_event_gettype(gev));

		// just in case we get into some crazy loop.....
		sleep(1);
		break;
	}

	return EP_STAT_OK;
}


void
multiread_cb(gdp_event_t *gev)
{
	(void) multiread_print_event(gev, false);
	gdp_event_free(gev);
}


/*
**  DO_MULTIREAD --- subscribe or multiread
**
**		This routine handles calls that return multiple values via the
**		event interface.  They might include subscriptions.
*/

EP_STAT
do_multiread(gdp_gcl_t *gcl,
		gdp_recno_t firstrec,
		int32_t numrecs,
		bool subscribe,
		bool use_callbacks)
{
	EP_STAT estat;
	void (*cbfunc)(gdp_event_t *) = NULL;

	if (use_callbacks)
		cbfunc = multiread_cb;

	if (subscribe)
	{
		// start up a subscription
		estat = gdp_gcl_subscribe(gcl, firstrec, numrecs, NULL, cbfunc, NULL);
	}
	else
	{
		// make the flags more user-friendly
		if (firstrec == 0)
			firstrec = 1;

		// start up a multiread
		estat = gdp_gcl_multiread(gcl, firstrec, numrecs, cbfunc, NULL);
	}

	// check to make sure the subscribe/multiread succeeded; if not, bail
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[200];

		ep_app_fatal("Cannot %s:\n\t%s",
				subscribe ? "subscribe" : "multiread",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// this sleep will allow multiple results to appear before we start reading
	if (ep_dbg_test(Dbg, 100))
		ep_time_nanosleep(500000000);	//DEBUG: one half second

	// now start reading the events that will be generated
	if (!use_callbacks)
	{
		for (;;)
		{
			// get the next incoming event
			gdp_event_t *gev = gdp_event_next(NULL, 0);

			// print it
			estat = multiread_print_event(gev, subscribe);

			// don't forget to free the event!
			gdp_event_free(gev);

			EP_STAT_CHECK(estat, break);
		}
	}
	else
	{
		// hang for an hour waiting for events
		sleep(3600);
	}

	return estat;
}


/*
**  PRINT_METADATA --- get and print the metadata
*/

void
print_metadata(gdp_gcl_t *gcl)
{
	EP_STAT estat;
	gdp_gclmd_t *gmd;

	estat = gdp_gcl_getmetadata(gcl, &gmd);
	EP_STAT_CHECK(estat, goto fail0);

	gdp_gclmd_print(gmd, stdout, 5);
	gdp_gclmd_free(gmd);
	return;

fail0:
	{
		char ebuf[100];

		fprintf(stderr, "Could not read metadata!\n    %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}

void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-c] [-D dbgspec] [-f firstrec] [-G router_addr] [-m]\n"
			"  [-L logfile] [-M] [-n nrecs] [-s] [-t] [-v] log_name\n"
			"    -c  use callbacks\n"
			"    -D  turn on debugging flags\n"
			"    -f  first record number to read (from 1)\n"
			"    -G  IP host to contact for gdp_router\n"
			"    -L  set logging file name (for debugging)\n"
			"    -m  use multiread\n"
			"    -M  show log metadata\n"
			"    -n  set number of records to read (default all)\n"
			"    -q  be quiet (don't print any metadata)\n"
			"    -s  subscribe to this log\n"
			"    -t  print data as text (instead of hexdump)\n"
			"    -v  print verbose output (include signature)\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}

/*
**  MAIN --- the name says it all
*/

int
main(int argc, char **argv)
{
	gdp_gcl_t *gcl;
	EP_STAT estat;
	char buf[200];
	gdp_name_t gclname;
	int opt;
	char *gdpd_addr = NULL;
	bool subscribe = false;
	bool multiread = false;
	bool use_callbacks = false;
	bool showmetadata = false;
	int32_t numrecs = 0;
	gdp_recno_t firstrec = 0;
	bool show_usage = false;
	char *log_file_name = NULL;
	gdp_iomode_t open_mode = GDP_MODE_RO;

	setlinebuf(stdout);								//DEBUG
	//char outbuf[65536];							//DEBUG
	//setbuffer(stdout, outbuf, sizeof outbuf);		//DEBUG

	// parse command-line options
	while ((opt = getopt(argc, argv, "AcD:f:G:L:mMn:qstv")) > 0)
	{
		switch (opt)
		{
		  case 'A':				// hidden flag for debugging only
			open_mode = GDP_MODE_RA;
			break;

		  case 'c':
			// use callbacks
			use_callbacks = true;
			break;

		  case 'D':
			// turn on debugging
			ep_dbg_set(optarg);
			break;

		  case 'f':
			// select the first record
			firstrec = atol(optarg);
			break;

		  case 'G':
			// set the port for connecting to the GDP daemon
			gdpd_addr = optarg;
			break;

		  case 'L':
			log_file_name = optarg;
			break;

		  case 'm':
			// turn on multi-read (see also -s)
			multiread = true;
			break;

		  case 'M':
			showmetadata = true;
			break;

		  case 'n':
			// select the number of records to be returned
			numrecs = atol(optarg);
			break;

		  case 'q':
			// be quiet (don't print metadata)
			Quiet = true;
			break;

		  case 's':
			// subscribe to this GCL (see also -m)
			subscribe = true;
			break;

		  case 't':
			// print data as text
			TextData = true;
			break;

		  case 'v':
			PrintSig = true;
			break;

		  default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	// we require a GCL name
	if (show_usage || argc <= 0)
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
	ep_time_nanosleep(INT64_C(100000000));		// 100 msec

	// parse the name (either base64-encoded or symbolic)
	estat = gdp_parse_name(argv[0], gclname);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_fatal("illegal GCL name syntax:\n\t%s", argv[0]);
		exit(EX_USAGE);
	}

	// convert it to printable format and tell the user what we are doing
	if (!Quiet)
	{
		gdp_pname_t gclpname;

		gdp_printable_name(gclname, gclpname);
		fprintf(stderr, "Reading GCL %s\n", gclpname);
	}

	// open the GCL; arguably this shouldn't be necessary
	estat = gdp_gcl_open(gclname, open_mode, NULL, &gcl);
	if (!EP_STAT_ISOK(estat))
	{
		char sbuf[100];

		ep_app_error("Cannot open GCL:\n    %s",
				ep_stat_tostr(estat, sbuf, sizeof sbuf));
		goto fail0;
	}

	if (showmetadata)
		print_metadata(gcl);

	// arrange to do the reading via one of the helper routines
	if (subscribe || multiread || use_callbacks)
		estat = do_multiread(gcl, firstrec, numrecs, subscribe, use_callbacks);
	else
		estat = do_simpleread(gcl, firstrec, numrecs);

	// might as well let the GDP know we're going away
	gdp_gcl_close(gcl);

fail0:
	// might as well let the user know what's going on....
	if (!Quiet || EP_STAT_SEVERITY(estat) > EP_STAT_SEV_WARN)
		fprintf(stderr, "exiting after %d records with status %s\n",
				NRead, ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
