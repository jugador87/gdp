/*
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
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
**	----- END LICENSE BLOCK -----
*/

#include <gdp/gdp_zc_client.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
	zlist_t list;

	printf("start browse\n");
	while (1) {
		if (gdp_zc_scan())
		{
			/* always need to retrieve list */
			printf("getting info\n");
			gdp_zc_list(&list);
			printf("\n");

			/* you can access the info as a SLIST */
			zentry_t *np;
			SLIST_FOREACH(np, &list.head, entries)
			{
				printf("host: %s port: %d\n", np->address, np->port);
			}
			printf("\n");

			/* or you can access the info as a string */
			char zcstr[gdp_zc_strlen(&list)];
			gdp_zc_str(&list, zcstr);
			printf("list: %s\n", zcstr);
			printf("\n");

			/* you always need to free the list after you're done */
			printf("freeing all zeroconf\n");
			gdp_zc_free();
			return 0;
		}
		printf("scan failed.");
		sleep(5);
		printf(" retrying...\n");
	}
}


/* vim: set noexpandtab : */
