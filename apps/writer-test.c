/* vim: set ai sw=4 sts=4 : */

#include <gdp/gdp_nexus.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char **argv)
{
    nexdle_t *nexdle;
    char *nexpname = NULL;
    int opt;
    EP_STAT estat;
    long msgno = 1;
    char buf[200];

    while ((opt = getopt(argc, argv, "a:D:")) > 0)
    {
	switch (opt)
	{
	 case 'a':
	    nexpname = optarg;
	    break;

	 case 'D':
	    ep_dbg_set(optarg);
	    break;
	}
    }

    if (nexpname == NULL)
    {
	// create a new nexdle
	estat = gdp_nexus_create(NULL, &nexdle);
    }
    else
    {
	nname_t nexiname;

	gdp_nexus_internal_name(nexpname, nexiname);
	estat = gdp_nexus_open(nexiname, GDP_MODE_AO, &nexdle);
    }
    EP_STAT_CHECK(estat, goto fail1);
    gdp_nexus_print(nexdle, stdout, 0, 0);
    fprintf(stdout, "\nStarting to read input\n");

    while (fgets(buf, sizeof buf, stdin) != NULL)
    {
	char *p = strchr(buf, '\n');
	nexmsg_t msg;

	if (p != NULL)
	    *p++ = '\0';

	fprintf(stdout, "Got input %s%s%s\n", EpChar->lquote, buf,
		EpChar->rquote);
	memset(&msg, '\0', sizeof msg);
	msg.data = buf;
	msg.len = strlen(buf);
	msg.msgno = msgno++;

	estat = gdp_nexus_append(nexdle, &msg);
	EP_STAT_CHECK(estat, goto fail1);
    }

fail1:
    fprintf(stderr, "exiting with status %s\n",
	    ep_stat_tostr(estat, buf, sizeof buf));
    return !EP_STAT_ISOK(estat);
}
