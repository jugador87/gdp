/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  ----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**  ----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <locale.h>
#include <ctype.h>
#include <arpa/inet.h>

#include <time.h>

#include "gdp_zc_client.h"

static EP_DBG	DemoMode = EP_DBG_INIT("_demo", "Demo Mode");

typedef enum
{
	COMMAND_HELP,
	COMMAND_VERSION,
	COMMAND_BROWSE_SERVICES,
	COMMAND_BROWSE_ALL_SERVICES,
	COMMAND_BROWSE_DOMAINS,
#if defined(HAVE_GDBM) || defined(HAVE_DBM)
	COMMAND_DUMP_STDB,
#endif
} command;

typedef struct config
{
	int verbose;
	int terminate_on_all_for_now;
	int terminate_on_cache_exhausted;
	char *domain;
	char *stype;
	int ignore_local;
	command comm;
	int resolve;
	int no_fail;
	int parsable;
#if defined(HAVE_GDBM) || defined(HAVE_DBM)
	int no_db_lookup;
#endif
} config_t;

typedef struct service_info service_info_t;

struct service_info
{
	AvahiIfIndex interface;
	AvahiProtocol protocol;
	char *name, *type, *domain;
	AvahiServiceResolver *resolver;
	config_t *conf;
	AVAHI_LLIST_FIELDS(service_info_t, info);
};

static AvahiSimplePoll *SimplePoll = NULL;
static AvahiClient *Client = NULL;
static int NAllForNow = 0, NCacheExhausted = 0, NResolving = 0;
static AvahiStringList *BrowsedTypes = NULL;
static service_info_t *Services = NULL;
static int NColumns = 80;
static int Browsing = 0;
static zlist_t *ZList = NULL;

/*
 * This will modify the passed in pointer
 *
 * You need to allocate memory for it.
 */
static void
zc_list_init(zlist_t *list)
{
	SLIST_INIT(&list->head);
	list->len = 0;
}

/*
 * Adds element to the head of list
 */
static void
zc_list_push(zlist_t *list, zentry_t *entry)
{
	SLIST_INSERT_HEAD(&list->head, entry, entries);
	list->len++;
}

/*
 * Removes and returns first element of the list
 *
 * The caller needs to free the returned value
 */
static zentry_t *
zc_list_pop(zlist_t *list)
{
	struct zentry *node = SLIST_FIRST(&list->head);
	SLIST_REMOVE_HEAD(&list->head, entries);
	list->len--;
	return node;
}

/*
 * Returns whether or not the list is empty
 */
static int
zc_list_isempty(zlist_t *list)
{
	return SLIST_EMPTY(&list->head);
}

/*
 * Completely frees a list
 */
static void
zc_list_free(zlist_t *list)
{
	zentry_t *np;

	while (!SLIST_EMPTY(&list->head))
	{
		np = SLIST_FIRST(&list->head);
		SLIST_REMOVE_HEAD(&list->head, entries);
		avahi_free(np->address);
		avahi_free(np);
	}
	avahi_free(list);
}

/*
 * Fisher-Yates shuffle algorithm
 * 
 * Shuffle array of pointers to zentry_t
 * 
 * a: array
 * n: length
 */
static void
shuffle_zentry(zentry_t *a[], int n)
{
	int i, j;
	zentry_t *tmp;

	srand(time(NULL));

	for (i = n - 1; i > 0; i--)
	{
		j = rand() % (i + 1);
		tmp = a[i];
		a[i] = a[j];
		a[j] = tmp;
	}
}

/*
 * Puts a shuffled array of pointers containing the contents of the list
 * into dst
 *
 * It's a shallow copy
 */
static void
shuffled_array(zlist_t *list, zentry_t *dst[])
{
	zentry_t *np;
	int i = 0;

	SLIST_FOREACH(np, &list->head, entries)
	{
		dst[i] = np;
		i++;
	}

	shuffle_zentry(dst, list->len);
}

/*
 * Shuffles the list
 *
 * Breaks abstraction on the zlist_t
 */
static void
shuffle_list(zlist_t *list)
{
	zentry_t *np;
	zentry_t *arr[list->len];
	int i;

	shuffled_array(list, arr);

	while (!SLIST_EMPTY(&list->head))
	{
		np = SLIST_FIRST(&list->head);
		SLIST_REMOVE_HEAD(&list->head, entries);
	}

	for (i = 0; i < list->len; i++)
	{
		SLIST_INSERT_HEAD(&list->head, arr[i], entries);
	}
}

static char *
make_printable(const char *from, char *to)
{
	const char *f;
	char *t;

	for (f = from, t = to; *f; f++, t++)
		*t = isprint(*f) ? *f : '_';

	*t = 0;

	return to;
}

/*Print something about zeroconf.  It appears to be only for debugging.
 */
static void
print_service_line(config_t *conf,
		char c,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		const char *name,
		const char *type,
		const char *domain,
		int nl)
{
	char ifname[IF_NAMESIZE];

#if defined(HAVE_GDBM) || defined(HAVE_DBM)
	if (!conf->no_db_lookup)
		type = stdb_lookup(type);
#endif

	if (conf->parsable)
	{
		char sn[AVAHI_DOMAIN_NAME_MAX], *e = sn;
		size_t l = sizeof(sn);

		printf("%c;%s;%s;%s;%s;%s%s",
			c,
			interface != AVAHI_IF_UNSPEC ? if_indextoname(interface, ifname) : "n/a",
			protocol != AVAHI_PROTO_UNSPEC ? avahi_proto_to_string(protocol) : "n/a",
			avahi_escape_label(name, strlen(name), &e, &l), type, domain, nl ? "\n" : "");
	}
	else
	{
		char label[AVAHI_LABEL_MAX];
		make_printable(name, label);

		printf("%c %6s %4s %-*s %-20s %s\n",
			c,
			interface != AVAHI_IF_UNSPEC ? if_indextoname(interface, ifname) : "n/a",
			protocol != AVAHI_PROTO_UNSPEC ? avahi_proto_to_string(protocol) : "n/a",
			NColumns - 35, label, type, domain);
	}
	fflush(stdout);
}

static void
remove_service(config_t *c, service_info_t *i)
{
	assert(c);
	assert(i);

	AVAHI_LLIST_REMOVE(service_info_t, info, Services, i);

	if (i->resolver)
		avahi_service_resolver_free(i->resolver);

	avahi_free(i->name);
	avahi_free(i->type);
	avahi_free(i->domain);
	avahi_free(i);
}

static void
check_terminate(config_t *c)
{
	assert(NAllForNow >= 0);
	assert(NCacheExhausted >= 0);
	assert(NResolving >= 0);

	if (NAllForNow <= 0 && NResolving <= 0)
	{

		if (c->verbose && !c->parsable)
		{
			printf(": All for now\n");
			NAllForNow++; /* Make sure that this event is not repeated */
		}

		if (c->terminate_on_all_for_now)
		{
			avahi_simple_poll_quit(SimplePoll);
		}
	}

	if (NCacheExhausted <= 0 && NResolving <= 0)
	{

		if (c->verbose && !c->parsable)
		{
			printf(": Cache exhausted\n");
			NCacheExhausted++; /* Make sure that this event is not repeated */
		}

		if (c->terminate_on_cache_exhausted)
		{
			avahi_simple_poll_quit(SimplePoll);
		}
	}
}

static void
service_resolver_callback(AvahiServiceResolver *r,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		AvahiResolverEvent event,
		const char *name,
		const char *type,
		const char *domain,
		const char *host_name,
		const AvahiAddress *a,
		uint16_t port,
		AvahiStringList *txt,
		AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
		void *userdata)
{
	service_info_t *i = userdata;

	assert(r);
	assert(i);

	switch (event)
	{
		case AVAHI_RESOLVER_FOUND:
		{
			char *t;
			char address[AVAHI_ADDRESS_STR_MAX];
			struct in_addr unused_address;
			zentry_t *entry = (zentry_t*) avahi_malloc(sizeof(zentry_t));

			avahi_address_snprint(address, sizeof(address), a);

			t = avahi_string_list_to_string(txt);

			if (i->conf->verbose)
			{
				print_service_line(i->conf, '=', interface, protocol,
						name, type, domain, 0);

				if (i->conf->parsable)
					printf(";%s;%s;%u;%s\n",
						host_name,
						address,
						port,
						t);
				else
					printf("   hostname = [%s]\n"
						"   address = [%s]\n"
						"   port = [%u]\n"
						"   txt = [%s]\n",
						host_name,
						address,
						port,
						t);
			}

			/* Assume that if it's not ipv6, then it's ipv4 */
			if (inet_pton(AF_INET6, address, &unused_address))
			{
				char format_addr[AVAHI_ADDRESS_STR_MAX+2];
				snprintf(format_addr, sizeof(format_addr), "[%s]", address);
				entry->address = avahi_strdup(format_addr);
			}
			else
			{
				entry->address = avahi_strdup(address);
			}

			/* Append to list */
			entry->port = port;
			zc_list_push(ZList, entry);
			avahi_free(t);
			break;
		}

		case AVAHI_RESOLVER_FAILURE:
			fprintf(stderr,
					"Failed to resolve service '%s' of type '%s' in domain '%s': %s\n",
					name, type, domain,
					avahi_strerror(avahi_client_errno(Client)));
			break;
	}


	avahi_service_resolver_free(i->resolver);
	i->resolver = NULL;

	assert(NResolving > 0);
	NResolving--;
	check_terminate(i->conf);
	fflush(stdout);
}

static service_info_t *
add_service(config_t *c,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		const char *name,
		const char *type,
		const char *domain)
{
	service_info_t *i;

	i = avahi_new(service_info_t, 1);

	if (c->resolve)
	{
		if (!(i->resolver = avahi_service_resolver_new(Client, interface,
						protocol, name, type, domain, AVAHI_PROTO_UNSPEC,
						0, service_resolver_callback, i)))
		{
			avahi_free(i);
			fprintf(stderr,
					"Failed to resolve service '%s' of type '%s' in domain '%s': %s\n",
					name, type, domain,
					avahi_strerror(avahi_client_errno(Client)));
			return NULL;
		}

		NResolving++;
	}
	else
	{
		i->resolver = NULL;
	}

	i->interface = interface;
	i->protocol = protocol;
	i->name = avahi_strdup(name);
	i->type = avahi_strdup(type);
	i->domain = avahi_strdup(domain);
	i->conf = c;

	AVAHI_LLIST_PREPEND(service_info_t, info, Services, i);

	return i;
}

static service_info_t *
find_service(AvahiIfIndex interface,
		AvahiProtocol protocol,
		const char *name,
		const char *type,
		const char *domain)
{
	service_info_t *i;

	for (i = Services; i; i = i->info_next)
		if (i->interface == interface &&
				i->protocol == protocol &&
				strcasecmp(i->name, name) == 0 &&
				avahi_domain_equal(i->type, type) &&
				avahi_domain_equal(i->domain, domain))
			return i;
	return NULL;
}

static void
service_browser_callback(AvahiServiceBrowser *b,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		AvahiBrowserEvent event,
		const char *name,
		const char *type,
		const char *domain,
		AvahiLookupResultFlags flags,
		void *userdata)
{

	config_t *c = userdata;

	assert(b);
	assert(c);

	switch (event)
	{
		case AVAHI_BROWSER_NEW:
		{
			if (c->ignore_local && (flags & AVAHI_LOOKUP_RESULT_LOCAL))
				break;

			if (find_service(interface, protocol, name, type, domain))
				return;

			add_service(c, interface, protocol, name, type, domain);

			if (c->verbose)
				print_service_line(c, '+', interface, protocol, name,
						type, domain, 1);
			break;

		}

		case AVAHI_BROWSER_REMOVE:
		{
			service_info_t *info;

			if (!(info = find_service(interface, protocol, name, type, domain)))
				return;

			remove_service(c, info);

			if (c->verbose)
				print_service_line(c, '-', interface, protocol, name,
						type, domain, 1);
			break;
		}

		case AVAHI_BROWSER_FAILURE:
			fprintf(stderr,
					"service_browser failed: %s\n",
					avahi_strerror(avahi_client_errno(Client)));
			avahi_simple_poll_quit(SimplePoll);
			break;

		case AVAHI_BROWSER_CACHE_EXHAUSTED:
			NCacheExhausted --;
			check_terminate(c);
			break;

		case AVAHI_BROWSER_ALL_FOR_NOW:
			NAllForNow --;
			check_terminate(c);
			break;
	}
}

static void
browse_service_type(config_t *c,
		const char *stype,
		const char *domain)
{
	AvahiServiceBrowser *b;
	AvahiStringList *i;

	assert(c);
	assert(Client);
	assert(stype);

	for (i = BrowsedTypes; i; i = i->next)
		if (avahi_domain_equal(stype, (char*) i->text))
			return;

	if (!(b = avahi_service_browser_new(
			Client,
			AVAHI_IF_UNSPEC,
			AVAHI_PROTO_UNSPEC,
			stype,
			domain,
			0,
			service_browser_callback,
			c)))
	{
		fprintf(stderr,
				"avahi_service_browser_new() failed: %s\n",
				avahi_strerror(avahi_client_errno(Client)));
		avahi_simple_poll_quit(SimplePoll);
	}

	BrowsedTypes = avahi_string_list_add(BrowsedTypes, stype);

	NAllForNow++;
	NCacheExhausted++;
}

static int
start(config_t *conf)
{

	assert(!Browsing);

	const char *version, *hn;

	if (!(version = avahi_client_get_version_string(Client)))
	{
		fprintf(stderr,
				"Failed to query version string: %s\n",
				avahi_strerror(avahi_client_errno(Client)));
		return -1;
	}

	if (!(hn = avahi_client_get_host_name_fqdn(Client)))
	{
		fprintf(stderr,
				"Failed to query host name: %s\n",
				avahi_strerror(avahi_client_errno(Client)));
		return -1;
	}

	browse_service_type(conf, conf->stype, conf->domain);

	Browsing = 1;
	return 0;
}

static int create_new_simple_poll_client(config_t *conf);

static void
client_callback(AvahiClient *c,
		AvahiClientState state,
		AVAHI_GCC_UNUSED void * userdata)
{
	config_t *conf = userdata;

	/*
		This function might be called when avahi_client_new() has not
		returned yet.
	*/
	Client = c;

	switch (state)
	{
		case AVAHI_CLIENT_FAILURE:
			if (conf->no_fail && avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED)
			{
				int error;

				/* We have been disconnected, so let reconnect */

				fprintf(stderr, "Disconnected, reconnecting ...\n");

				avahi_client_free(Client);
				Client = NULL;

				avahi_string_list_free(BrowsedTypes);
				BrowsedTypes = NULL;

				while (Services)
					remove_service(conf, Services);

				Browsing = 0;

				error = create_new_simple_poll_client(conf);
			}
			else
			{
				fprintf(stderr,
						"Client failure, exiting: %s\n",
						avahi_strerror(avahi_client_errno(c)));
				avahi_simple_poll_quit(SimplePoll);
			}
			break;

		case AVAHI_CLIENT_S_REGISTERING:
		case AVAHI_CLIENT_S_RUNNING:
		case AVAHI_CLIENT_S_COLLISION:
			if (!Browsing)
				if (start(conf) < 0)
				{
					avahi_simple_poll_quit(SimplePoll);
				}
			break;

		case AVAHI_CLIENT_CONNECTING:
			if (conf->verbose && !conf->parsable)
				fprintf(stderr, "Waiting for daemon ...\n");
			break;
	}
}

static int
init_config(config_t *c)
{
	assert(c);

	c->terminate_on_cache_exhausted = 0;
	c->terminate_on_all_for_now = 0;
	c->ignore_local = 0;
	c->no_fail = 0;
	c->parsable = 0;
	c->verbose = ep_dbg_test(DemoMode, 1);
	c->resolve = 1;
	c->terminate_on_all_for_now = 1;

	c->stype = avahi_strdup(ep_adm_getstrparam("swarm.gdp.zeroconf.proto",
				"_gdp._tcp"));
	c->domain = avahi_strdup(ep_adm_getstrparam("swarm.gdp.zeroconf.domain",
				"local"));

	return 0;
}

static int
create_new_simple_poll_client(config_t *conf)
{
	int error = 0;

	Client = avahi_client_new(avahi_simple_poll_get(SimplePoll),
			conf->no_fail ? AVAHI_CLIENT_NO_FAIL : 0,
			client_callback,
			conf,
			&error);
	if (Client == NULL)
	{
		if (conf->verbose)
			fprintf(stderr, "Failed to create client object: %s\n",
					avahi_strerror(error));
		avahi_simple_poll_quit(SimplePoll);
	}
	return error;
}

/////////////////////// Public API below ///////////////////////

void
gdp_zc_list(zlist_t *dst)
{
	*dst = *ZList;
}

size_t
gdp_zc_strlen(zlist_t *list)
{
	return (ZC_MAX_ADDR_LEN + 2) * list->len + 1;
}

void
gdp_zc_str(zlist_t *list, char dst[])
{
	zentry_t *np;
	char buf[ZC_MAX_ADDR_LEN + 3];

	dst[0] = '\0';
	SLIST_FOREACH(np, &list->head, entries)
	{
		snprintf(buf, sizeof(buf), "%s:%u;", np->address, np->port);
		strlcat(dst, buf, gdp_zc_strlen(list));
	}
}

void
gdp_zc_free()
{
	zc_list_free(ZList);
}

int
gdp_zc_scan()
{
	int error;
	config_t conf;

	init_config(&conf);

	if (!(SimplePoll = avahi_simple_poll_new()))
	{
		fprintf(stderr, "Failed to create simple poll object.\n");
		goto fail0;
	}

	error = create_new_simple_poll_client(&conf);
	if (error != 0)
	{
		goto fail1;
	}


	ZList = (zlist_t *) avahi_malloc(sizeof(zlist_t));
	zc_list_init(ZList);
	avahi_simple_poll_loop(SimplePoll);
	shuffle_list(ZList);
	return 1;

fail1:
	while (Services)
	{
		remove_service(&conf, Services);
	}
	if (Client)
	{
		avahi_client_free(Client);
		Client = NULL;
	}
	if (SimplePoll)
	{
		avahi_simple_poll_free(SimplePoll);
		SimplePoll = NULL;
	}

fail0:
	avahi_free(conf.domain);
	avahi_free(conf.stype);
	return 0;
}

/* vim: set noexpandtab : */
