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

#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/domain.h>
#include <avahi-common/llist.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <sys/queue.h>

#define ZC_MAX_PORT_LEN 5
/* The +2 is for the brackets in an ipv6 address */
#define ZC_MAX_ADDR_LEN ((AVAHI_ADDRESS_STR_MAX) + 2)

typedef SLIST_HEAD(zlisthead, zentry) zlisthead_t;

typedef struct
zentry
{
	char *address;
	uint16_t port;
	SLIST_ENTRY(zentry) entries;
} zentry_t;

typedef struct
zlist
{
	size_t len;
	zlisthead_t head;
} zlist_t;

/*
 * This runs Zeroconf.
 *
 * Return 1 on success and 0 on failure.
 */
int gdp_zc_scan();

/*
 * Get the list of all info found by Zeroconf.
 *
 * The list will be put in dst.
 * You can iterate through it as a SLIST.
 *
 * DO NOT modify the contents of this list.
 */
void gdp_zc_list(zlist_t *dst);

/*
 * Get all info from the list as a string.
 *
 * How to use:
 * Pass in the list you want to read as a string. The output will be put in
 * dst. Only "whole" entries will be copied into dst, where a "whole" entry
 * is defined as "ipaddr:port;"
 *
 * At most len-1 characters will be copied into dst where the last
 * character will be the null character '\0'.
 */
void gdp_zc_str(zlist_t *list, char *dst, size_t len);

/*
 * Returns the max buffer size gdp_zc_str() might use.
 */
size_t gdp_zc_str_bufsize(zlist_t *list);

/*
 * Does all zeroconf cleanup.
 *
 * This includes all the elements of the list. All accessor functions only
 * point to the internal nodes so you should not be freeing those on your own.
 */
void gdp_zc_free();


/* vim: set noexpandtab : */
