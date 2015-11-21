/*
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

#include <gdp/gdp_zc_client.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
	ZCInfo *i, **list;
	char *zcstr;

	printf("start browse\n");
	if (gdp_zc_scan()) {
		printf("getting info\n");
		/* always need to retrieve list */
		list = gdp_zc_get_infolist();

		/* you can access info as a linked list */
		for (i = *list; i; i = i->info_next) {
			printf("host:%s port: %d\n", i->address, i->port);
		}

		/* or you can access info as a string */
		zcstr = gdp_zc_addr_str(list);
		if (zcstr) {
			printf("list: %s\n", zcstr);
			/* need to free the string after you're done */
			free(zcstr);
		} else {
			printf("list fail\n");
		}

		/* you always need to free the list after you're done */
		printf("freeing info\n");
		gdp_zc_free_infolist(list);
		return 0;
	} else {
		return 1;
	}
}

/* vim: set noexpandtab : */
