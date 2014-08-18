/* vim: set ai sw=4 sts=4 ts=4 : */

#include <gdp/gdp.h>
#include <ep/ep_dbg.h>
#include <ep/ep_app.h>
#include <event2/buffer.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
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
	uint32_t recno;
	gdp_msg_t *msg;

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

	msg = gdp_msg_new();

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

	recno = 1;
	for (;;)
	{
		estat = gdp_gcl_read(gclh, recno, msg);
		EP_STAT_CHECK(estat, break);
		fprintf(stdout, "  >>> ");
		gdp_msg_print(msg, stdout);
		recno++;

		// flush any left over data
		if (gdp_buf_reset(msg->dbuf) < 0)
			printf("*** WARNING: buffer reset failed: %s\n",
					strerror(errno));
	}

fail0:
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
