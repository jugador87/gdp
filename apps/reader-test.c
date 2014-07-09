/* vim: set ai sw=4 sts=4 : */

#include <gdp/gdp.h>
#include <ep/ep_dbg.h>
#include <ep/ep_app.h>
#include <event2/buffer.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char **argv)
{
    gcl_handle_t *gclh;
    EP_STAT estat;
    char buf[200];
    gcl_name_t gclname;
    char *gclpname;
    int opt;
    uint32_t msgno;
    struct evbuffer *evb;

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

    if (argc <= 0)
    {
	fprintf(stderr, "Usage: %s [-D dbgspec] <gcl_name>\n",
		ep_app_getprogname());
	exit(1);
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
	fprintf(stderr, "illegal GCL name syntax: %s\n", gclpname);
	exit(1);
    }

    estat = gdp_gcl_open(gclname, GDP_MODE_RO, &gclh);
    gdp_gcl_print(gclh, stderr, 0, 0);
    if (!EP_STAT_ISOK(estat))
    {
	char sbuf[100];

	ep_app_error("Cannot open GCL: %s",
		ep_stat_tostr(estat, sbuf, sizeof sbuf));
	goto fail0;
    }

    evb = evbuffer_new();

    msgno = 1;
    for (;;)
    {
	gdp_msg_t msg;
	char mbuf[1024];

	estat = gdp_gcl_read(gclh, msgno, &msg, evb);
	EP_STAT_CHECK(estat, break);
	msg.len = evbuffer_remove(evb, mbuf, sizeof mbuf);
	msg.data = mbuf;
	gdp_gcl_msg_print(&msg, stdout);
	msgno++;

	// flush any left over data
	evbuffer_drain(evb, UINT32_MAX);
    }

fail0:
    fprintf(stderr, "exiting with status %s\n",
	    ep_stat_tostr(estat, buf, sizeof buf));
    return !EP_STAT_ISOK(estat);
}
