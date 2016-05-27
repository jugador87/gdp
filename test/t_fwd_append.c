/* vim: set ai sw=4 sts=4 ts=4 : */

/*  To compile:
cc -I.. t_fwd_append.c -Lep -Lgdp -lgdp -lep -levent -levent_pthreads -pthread -lcrypto -lavahi-client -lavahi-common
*/

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_time.h>

#include <getopt.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

static EP_DBG	Dbg = EP_DBG_INIT("fwd_append", "GDP forwarded append test");



/*
**  CREATE_DATUM --- create sample datum for "forwarding"
*/

gdp_datum_t *
create_datum(gdp_recno_t recno)
{
	gdp_datum_t *datum = gdp_datum_new();

	datum->recno = recno;
	gdp_buf_write(gdp_datum_getbuf(datum), "test", 4);

	return datum;
}



/*
**  DO_FWD_APPEND --- fake a forwarded append operation
**
**		This routine handles calls that return multiple values via the
**		event interface.
*/

EP_STAT
do_fwd_append(gdp_gcl_t *gcl,
		gdp_datum_t *datum,
		gdp_name_t svrname,
		void *udata)
{
	EP_STAT estat;

	// start up a fwd_append
	estat = _gdp_gcl_fwd_append(gcl, datum, _GdpChannel, 0, svrname);

	// check to make sure the fwd_append succeeded; if not, bail
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[200];

		ep_app_fatal("Cannot fwd_append:\n\t%s",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	// this sleep will allow multiple results to appear before we start reading
	if (ep_dbg_test(Dbg, 100))
		ep_time_nanosleep(500000000);	//DEBUG: one half second

	return estat;
}


void
usage(void)
{
	fprintf(stderr, "Usage: %s [-D dbgspec] log_name server_name\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	gdp_gcl_t *gcl;
	EP_STAT estat;
	char ebuf[60];
	int opt;
	gdp_recno_t recno;

	while ((opt = getopt(argc, argv, "D:")) > 0)
	{
		switch (opt)
		{
		  case 'D':
			ep_dbg_set(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();
	char *log_xname = argv[0];
	char *svr_xname = argv[1];
	argc -= 2;
	argv += 2;

	ep_app_info("Forwarding append to log %s on server %s",
			log_xname, svr_xname);

	estat = gdp_init(NULL);
	ep_app_info("gdp_init: %s", ep_stat_tostr(estat, ebuf, sizeof ebuf));

	ep_time_nanosleep(INT64_C(100000000));

	gdp_name_t gclname;
	estat = gdp_parse_name(log_xname, gclname);
	ep_app_info("gdp_parse_name(%s): %s", log_xname,
			ep_stat_tostr(estat, ebuf, sizeof ebuf));

	gdp_name_t svrname;
	estat = gdp_parse_name(svr_xname, svrname);
	ep_app_info("gdp_parse_name(%s): %s", svr_xname,
			ep_stat_tostr(estat, ebuf, sizeof ebuf));

	estat = gdp_gcl_open(gclname, GDP_MODE_RA, NULL, &gcl);
	ep_app_info("gdp_gcl_open: %s", ep_stat_tostr(estat, ebuf, sizeof ebuf));

	recno = gdp_gcl_getnrecs(gcl);
	gdp_datum_t *datum = create_datum(++recno);
	estat = do_fwd_append(gcl, datum, svrname, NULL);
	ep_app_info("do_fwd_append: %s", ep_stat_tostr(estat, ebuf, sizeof ebuf));

	// collect results
	gdp_event_t *gev;
	while ((gev = gdp_event_next(gcl, NULL)) != NULL)
	{
		gdp_event_print(gev, stdout, 3);
		if (gdp_event_gettype(gev) == GDP_EVENT_EOS ||
				gdp_event_gettype(gev) == GDP_EVENT_CREATED)
			break;
		gdp_event_free(gev);
	}
	exit(EX_OK);
}
