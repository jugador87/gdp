/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  Log Advertisements
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

		ep_dbg_printf("\t%s\n", gdp_printable_name(gname, pname));
	}

	i = gdp_buf_write(b, gname, sizeof (gdp_name_t));
	if (i < 0)
		ep_dbg_cprintf(Dbg, 1, "logd_adv_addone: gdp_buf_write failure\n");
}


static EP_STAT
advertise_all(gdp_buf_t *dbuf, void *ctx)
{
	gcl_physforeach(adv_addone, dbuf);
	return EP_STAT_OK;
}


void
logd_advertise_all(void)
{
	EP_STAT estat = _gdp_advertise(advertise_all, NULL);
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];

		ep_dbg_printf("logd_advertise_all => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}


/*
**  Advertise a new GCL
*/

static EP_STAT
advertise_one(gdp_buf_t *dbuf, void *ctx)
{
	gdp_buf_write(dbuf, ctx, sizeof (gdp_name_t));
	return EP_STAT_OK;
}

void
logd_advertise_one(gdp_name_t gname)
{
	EP_STAT estat = _gdp_advertise(advertise_one, gname);
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];
		gdp_pname_t pname;

		ep_dbg_printf("logd_advertise_one(%s) =>\n\t%s\n",
				gdp_printable_name(gname, pname),
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
}
