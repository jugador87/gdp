/* vim: set ai sw=4 sts=4 : */

#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <unistd.h>
#include <string.h>


int
main(int argc, char **argv)
{
    gcl_handle_t *gclh;
    char *gclpname = NULL;
    int opt;
    EP_STAT estat;
    long msgno = 1;
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

    estat = gdp_init(true);
    if (!EP_STAT_ISOK(estat))
    {
	ep_app_error("GDP Initialization failed");
	goto fail0;
    }

    if (gclpname == NULL)
    {
	// create a new GCL handle
	estat = gdp_gcl_create(NULL, NULL, &gclh);
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

    while (fgets(buf, sizeof buf, stdin) != NULL)
    {
	char *p = strchr(buf, '\n');
	gdp_msg_t msg;

	if (p != NULL)
	    *p++ = '\0';

	fprintf(stdout, "Got input %s%s%s\n", EpChar->lquote, buf,
		EpChar->rquote);
	memset(&msg, '\0', sizeof msg);
	msg.data = buf;
	msg.len = strlen(buf);
	msg.msgno = msgno++;

	estat = gdp_gcl_append(gclh, &msg);
	EP_STAT_CHECK(estat, goto fail1);
	gdp_gcl_msg_print(&msg, stdout);
    }
    goto done;

fail1:
    gdp_gcl_close(gclh);

fail0:
done:
    fprintf(stderr, "exiting with status %s\n",
	    ep_stat_tostr(estat, buf, sizeof buf));
    return !EP_STAT_ISOK(estat);
}
