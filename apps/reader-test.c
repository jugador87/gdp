/* vim: set ai sw=4 sts=4 : */

#include <gdp/gdp_nexus.h>
#include <ep/ep_dbg.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char **argv)
{
    nexdle_t *nexdle;
    EP_STAT estat;
    char buf[200];
    nname_t nname;
    char *nexuspname;
    int opt;

    while ((opt = getopt(argc, argv, "D:")) > 0)
    {
	switch (opt)
	{
	  case 'D':
//	    ep_dbg_set(optarg);
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc <= 0)
    {
	fprintf(stderr, "Usage: %s [-D dbgspec] <nexus_name>\n",
		getprogname());
	exit(1);
    }
    nexuspname = argv[0];
    fprintf(stdout, "Reading nexus %s\n", nexuspname);

    if (gdp_nexus_internal_name(nexuspname, nname) < 0)
    {
	fprintf(stderr, "illegal nexus name syntax: %s\n", nexuspname);
	exit(1);
    }

    estat = gdp_nexus_open(nname, GDP_MODE_RO, &nexdle);
    EP_STAT_CHECK(estat, goto fail1);
    gdp_nexus_print(nexdle, stderr, 0, 0);

    for (;;)
    {
	nexmsg_t msg;
	char mbuf[1000];

	estat = gdp_nexus_read(nexdle, -1, &msg, mbuf, sizeof mbuf);
	EP_STAT_CHECK(estat, break);
	gdp_nexus_msg_print(&msg, stdout);

	// XXX should have some termination condition
    }

fail1:
    fprintf(stderr, "exiting with status %s\n",
	    ep_stat_tostr(estat, buf, sizeof buf));
    return !EP_STAT_ISOK(estat);
}
