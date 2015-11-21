/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  LOG-MIRROR --- mirror a log
**
**		Cheap and dirty replication.
**
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**	----- END LICENSE BLOCK -----
*/

#include <gdp/gdp.h>

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>

#include <sysexits.h>

void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbg_spec] [-G router_addr] source_log target_log\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for gdp_router\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	int opt;
	bool show_usage = false;
	EP_STAT estat;
	gdp_gcl_t *igcl, *ogcl;
	char *gdpd_addr = NULL;
	char buf[200];
	const char *lname, *lmode;
	gdp_recno_t nextrecno;
	gdp_name_t gcliname;
	gdp_event_t *gev;

	while ((opt = getopt(argc, argv, "D:G:")) > 0)
	{
		switch (opt)
		{
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

	if (show_usage || argc != 2)
		usage();

	// initialize GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	// open target GCL (must already exist)
	lname = argv[1];
	lmode = "append";
	gdp_parse_name(argv[1], gcliname);
	estat = gdp_gcl_open(gcliname, GDP_MODE_AO, NULL, &ogcl);
	EP_STAT_CHECK(estat, goto fail1);
	nextrecno = gdp_gcl_getnrecs(ogcl) + 1;

	// open a source GCL with the provided name
	lname = argv[0];
	lmode = "read";
	gdp_parse_name(argv[0], gcliname);
	estat = gdp_gcl_open(gcliname, GDP_MODE_RO, NULL, &igcl);
	EP_STAT_CHECK(estat, goto fail1);

	// subscribe to input starting from the first recno target does not have
	estat = gdp_gcl_subscribe(igcl, nextrecno, 0, NULL, NULL, NULL);
	EP_STAT_CHECK(estat, goto fail2);

	// copy forever
	while ((gev = gdp_event_next(igcl, NULL)) != NULL)
	{
		switch (gdp_event_gettype(gev))
		{
		 case GDP_EVENT_DATA:
			// copy the record to the new log
			estat = gdp_gcl_append(ogcl, gdp_event_getdatum(gev));
			EP_STAT_CHECK(estat, goto fail3);
			break;

		 case GDP_EVENT_EOS:
		 case GDP_EVENT_SHUTDOWN:
			ep_app_error("unexpected end of subscription");
			estat = EP_STAT_END_OF_FILE;
			goto fail2;

		 default:
			// ignore
			break;
		}
	}

fail3:
	ep_app_error("append failed");

fail2:
	// close GCLs / release resources
	//XXX someday

	if (false)
	{
fail1:
		ep_app_error("could not open %s for %s", lname, lmode);
	}

fail0:
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return EP_STAT_ISOK(estat) ? EX_OK : EX_UNAVAILABLE;
}
