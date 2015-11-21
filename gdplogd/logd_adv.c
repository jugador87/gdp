/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Log Advertisements
**
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
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


#include "logd.h"
#include "logd_physlog.h"

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>

#include <ep/ep_dbg.h>


static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.advertise",
							"GDP GCL Advertisements");


/*
**  Advertise all known GCLs
*/

static void
adv_addone(gdp_name_t gname, void *ctx)
{
	gdp_buf_t *b = ctx;
	int i;

	if (ep_dbg_test(Dbg, 54))
	{
		gdp_pname_t pname;

		ep_dbg_printf("\tAdvertise %s\n", gdp_printable_name(gname, pname));
	}

	i = gdp_buf_write(b, gname, sizeof (gdp_name_t));
	if (i < 0)
		ep_dbg_cprintf(Dbg, 1, "logd_adv_addone: gdp_buf_write failure\n");
}


static EP_STAT
advertise_all(gdp_buf_t *dbuf, void *ctx, int cmd)
{
	gcl_physforeach(adv_addone, dbuf);
	return EP_STAT_OK;
}


EP_STAT
logd_advertise_all(int cmd)
{
	EP_STAT estat = _gdp_advertise(advertise_all, NULL, cmd);
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];

		ep_dbg_printf("logd_advertise_all => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**  Advertise a new GCL
*/

static EP_STAT
advertise_one(gdp_buf_t *dbuf, void *ctx, int cmd)
{
	gdp_buf_write(dbuf, ctx, sizeof (gdp_name_t));
	return EP_STAT_OK;
}

void
logd_advertise_one(gdp_name_t gname, int cmd)
{
	EP_STAT estat = _gdp_advertise(advertise_one, gname, cmd);
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		gdp_pname_t pname;

		ep_dbg_printf("logd_advertise_one(%s) =>\n\t%s\n",
				gdp_printable_name(gname, pname),
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}
