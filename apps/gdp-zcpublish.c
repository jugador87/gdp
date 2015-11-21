/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	Advertise (publish) information about the GDP router
**
**		This should really be done automatically by the GDP Router
**		itself, but for the time being this is a stop-gap.
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
