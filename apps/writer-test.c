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


int
main(int argc, char **argv)
{
	gdp_gcl_t *gclh;
	gcl_name_t gcliname;
	int opt;
	EP_STAT estat;
	bool append = false;
	char *xname = NULL;
	char buf[200];

	while ((opt = getopt(argc, argv, "aD:")) > 0)
	{
		switch (opt)
		{
		 case 'a':
			append = true;
			break;

		 case 'D':
			ep_dbg_set(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
	{
		xname = argv[0];
		argc--;
		argv++;
	}
	if (argc != 0 || (append && xname == NULL))
	{
		fprintf(stderr, "Usage: %s [-D dbgspec] [-a] [<gcl_name>]\n"
				"  (name is required for -a)\n",
				ep_app_getprogname());
		exit(EX_USAGE);
	}

	estat = gdp_init();
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	if (xname == NULL)
	{
		// create a new GCL handle
		estat = gdp_gcl_create(NULL, &gclh);
	}
	else
	{
		gdp_gcl_parse_name(xname, gcliname);
		if (append)
			estat = gdp_gcl_open(gcliname, GDP_MODE_AO, &gclh);
		else
			estat = gdp_gcl_create(gcliname, &gclh);
	}
	EP_STAT_CHECK(estat, goto fail0);

	gdp_gcl_print(gclh, stdout, 0, 0);
	fprintf(stdout, "\nStarting to read input\n");

	gdp_datum_t *datum = gdp_datum_new();

	while (fgets(buf, sizeof buf, stdin) != NULL)
	{
		char *p = strchr(buf, '\n');

		if (p != NULL)
			*p++ = '\0';

		fprintf(stdout, "Got input %s%s%s\n", EpChar->lquote, buf,
				EpChar->rquote);
		gdp_buf_write(datum->dbuf, buf, strlen(buf));
		estat = gdp_gcl_publish(gclh, datum);
		EP_STAT_CHECK(estat, goto fail1);
		gdp_datum_print(datum, stdout);
	}
	gdp_datum_free(datum);
	goto done;

fail1:
	gdp_gcl_close(gclh);

fail0:
done:
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
