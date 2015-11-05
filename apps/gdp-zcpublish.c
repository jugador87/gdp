/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	Advertise (publish) information about the GDP router
**
**		This should really be done automatically by the GDP Router
**		itself, but for the time being this is a stop-gap.
**
*/


#define GDP_PORT_DEFAULT			8007

#include <gdp/gdp_zc_server.h>

#include <ep/ep.h>
#include <ep/ep_app.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>


void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-i instance] [-p port]\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	uint16_t port;
	char *instance = NULL;
	char instancebuf[120];
	int opt;

	ep_lib_init(0);
	ep_adm_readparams("gdp");
	port = ep_adm_getintparam("swarm.gdp.router.port", GDP_PORT_DEFAULT);
	while ((opt = getopt(argc, argv, "i:p:")) > 0)
	{
		switch (opt)
		{
		case 'i':				// instance name
			instance = optarg;
			break;

		case 'p':
			port = atoi(optarg);
			break;

		default:
			usage();
		}
	}

	if (instance == NULL)
	{
		char hostname[64];

		if (gethostname(hostname, sizeof hostname) < 0)
			ep_app_abort("cannot get host name");
		snprintf(instancebuf, sizeof instancebuf, "GDP Router on %s", hostname);
		instance = instancebuf;
	}

	printf("advertise gdp '%s' on %d\n", instance, port);
	gdp_zc_publish(instance, port);
	printf("do other stuff here\n");
	sleep(300);
	printf("terminating...\n");
	return 0;
}

/* vim: set noexpandtab : */
