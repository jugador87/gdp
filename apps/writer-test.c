/* vim: set ai sw=4 sts=4 ts=4 : */

#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>


int
main(int argc, char **argv)
{
	gdp_gcl_t *gclh;
	char *gclpname = NULL;
	int opt;
	EP_STAT estat;
	char buf[200];

	while ((opt = getopt(argc, argv, "a:D:")) > 0)
	{
		switch (opt)
		{
		 case 'a':
			gclpname = optarg;
			break;

		 case 'D':
			ep_dbg_set(optarg);
			break;
		}
	}

	estat = gdp_init();
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	if (gclpname == NULL)
	{
		// create a new GCL handle
		estat = gdp_gcl_create(NULL, &gclh);
	}
	else
	{
		gcl_name_t gcliname;

		gdp_gcl_internal_name(gclpname, gcliname);
		estat = gdp_gcl_open(gcliname, GDP_MODE_AO, &gclh);
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
