/*
**  ----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
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
**  ----- END LICENSE BLOCK -----
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "gdp_zc_server.h"

static AvahiEntryGroup *AGroup = NULL;
static AvahiSimplePoll *SimplePoll = NULL;
static char *SName = NULL;
static uint16_t SPort = 0;

static void create_services(AvahiClient *c);

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {
	assert(g == AGroup || AGroup == NULL);
	AGroup = g;

	/* Called whenever the entry group state changes */

	switch (state) {
		case AVAHI_ENTRY_GROUP_ESTABLISHED :
			/* The entry group has been established successfully */
			fprintf(stderr, "Service '%s' successfully established.\n", SName);
			break;

		case AVAHI_ENTRY_GROUP_COLLISION : {
			char *n;

			/* A service name collision with a remote service
			 * happened. Let's pick a new name */
			n = avahi_alternative_service_name(SName);
			avahi_free(SName);
			SName = n;

			fprintf(stderr, "Service name collision, renaming service to '%s'\n", SName);

			/* And recreate the services */
			create_services(avahi_entry_group_get_client(g));
			break;
		}

		case AVAHI_ENTRY_GROUP_FAILURE :

			fprintf(stderr, "Entry group failure: %s\n", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

			/* Some kind of failure happened while we were registering our services */
			avahi_simple_poll_quit(SimplePoll);
			break;

		case AVAHI_ENTRY_GROUP_UNCOMMITED:
		case AVAHI_ENTRY_GROUP_REGISTERING:
			;
	}
}

static void create_services(AvahiClient *c) {
	char *n, r[128];
	int ret;
	assert(c);

	/* If this is the first time we're called, let's create a new
	 * entry group if necessary */

	if (!AGroup)
		if (!(AGroup = avahi_entry_group_new(c, entry_group_callback, NULL))) {
			fprintf(stderr, "avahi_entry_group_new() failed: %s\n", avahi_strerror(avahi_client_errno(c)));
			goto fail;
		}

	/* If the group is empty (either because it was just created, or
	 * because it was reset previously, add our entries.  */

	if (avahi_entry_group_is_empty(AGroup)) {
		fprintf(stderr, "Adding service '%s'\n", SName);

		/* Create some random TXT data */
		snprintf(r, sizeof(r), "random=%i", rand());

		if ((ret = avahi_entry_group_add_service(AGroup, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, SName, "_gdp._tcp", NULL, NULL, SPort, NULL)) < 0) {

			if (ret == AVAHI_ERR_COLLISION)
				goto collision;

			fprintf(stderr, "Failed to add _gdp._tcp service: %s\n", avahi_strerror(ret));
			goto fail;
		}

		/* Tell the server to register the service */
		if ((ret = avahi_entry_group_commit(AGroup)) < 0) {
			fprintf(stderr, "Failed to commit entry group: %s\n", avahi_strerror(ret));
			goto fail;
		}
	}

	return;

collision:

	/* A service name collision with a local service happened. Let's
	 * pick a new name */
	n = avahi_alternative_service_name(SName);
	avahi_free(SName);
	SName = n;

	fprintf(stderr, "Service name collision, renaming service to '%s'\n", SName);

	avahi_entry_group_reset(AGroup);

	create_services(c);
	return;

fail:
	avahi_simple_poll_quit(SimplePoll);
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
	assert(c);

	/* Called whenever the client or server state changes */

	switch (state) {
		case AVAHI_CLIENT_S_RUNNING:

			/* The server has startup successfully and registered its host
			 * name on the network, so it's time to create our services */
			create_services(c);
			break;

		case AVAHI_CLIENT_FAILURE:

			fprintf(stderr, "Client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
			avahi_simple_poll_quit(SimplePoll);

			break;

		case AVAHI_CLIENT_S_COLLISION:

			/* Let's drop our registered services. When the server is back
			 * in AVAHI_SERVER_RUNNING state we will register them
			 * again with the new host name. */

		case AVAHI_CLIENT_S_REGISTERING:

			/* The server records are now being established. This
			 * might be caused by a host name change. We need to wait
			 * for our own records to register until the host name is
			 * properly esatblished. */

			if (AGroup)
				avahi_entry_group_reset(AGroup);

			break;

		case AVAHI_CLIENT_CONNECTING:
			;
	}
}

int gdp_zc_publish(const char *instance, uint16_t port_no) {
	AvahiClient *client = NULL;
	int error;
	int ret = 1;

	/* Allocate main loop object */
	if (!(SimplePoll = avahi_simple_poll_new())) {
		fprintf(stderr, "Failed to create simple poll object.\n");
		goto fail;
	}

	SName = avahi_strdup(instance);
	SPort = port_no;

	/* Allocate a new client */
	client = avahi_client_new(avahi_simple_poll_get(SimplePoll), 0, client_callback, NULL, &error);

	/* Check wether creating the client object succeeded */
	if (!client) {
		fprintf(stderr, "Failed to create client: %s\n", avahi_strerror(error));
		goto fail;
	}

	/* Run the main loop */
	avahi_simple_poll_loop(SimplePoll);

	ret = 0;

fail:

	/* Cleanup things */

	if (client)
		avahi_client_free(client);

	if (SimplePoll)
		avahi_simple_poll_free(SimplePoll);

	avahi_free(SName);

	return ret;
}

int gdp_zc_shutdown() {
	return 0;
}

/* vim: set noexpandtab : */
