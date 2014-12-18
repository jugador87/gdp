/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include "gdp.h"
#include "gdp_gclmd.h"
#include "gdp_stat.h"
#include "gdp_priv.h"

#include <event2/event.h>
#include <openssl/sha.h>

#include <errno.h>
#include <string.h>

/*
**	This implements the GDP API for C-based applications.
**
**	In the future this may need to be extended to have knowledge of
**	TSN/AVB, but for now we don't worry about that.
*/

/************************  PRIVATE	************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.api", "C API for GDP");



/***********************************************************************
**
**	Utility routines
**
***********************************************************************/

/*
**	GDP_GCL_GETNAME --- get the name of a GCL
*/

const gcl_name_t *
gdp_gcl_getname(const gdp_gcl_t *gcl)
{
	return &gcl->gcl_name;
}


/*
**  GDP_GCL_PRINTABLE_NAME --- make a printable GCL name from an internal name
**
**		Returns the external name buffer for ease-of-use.
*/

char *
gdp_gcl_printable_name(const gcl_name_t internal, gcl_pname_t external)
{
	EP_STAT estat = ep_b64_encode(internal, sizeof (gcl_name_t),
							external, sizeof (gcl_pname_t),
							EP_B64_ENC_URL);

	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 2,
				"gdp_gcl_printable_name: ep_b64_encode failure\n"
				"\tstat = %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		strcpy("(unknown)", external);
	}
	else if (EP_STAT_TO_INT(estat) != 43)
	{
		ep_dbg_cprintf(Dbg, 2,
				"gdp_gcl_printable_name: ep_b64_encode length failure (%d != 43)\n",
				EP_STAT_TO_INT(estat));
	}
	return external;
}

/*
**  GDP_GCL_INTERNAL_NAME --- parse a string GCL name to internal representation
*/

EP_STAT
gdp_gcl_internal_name(const gcl_pname_t external, gcl_name_t internal)
{
	EP_STAT estat;

	if (strlen(external) != GDP_GCL_PNAME_LEN)
	{
		estat = GDP_STAT_GCL_NAME_INVALID;
	}
	else
	{
		estat = ep_b64_decode(external, sizeof (gcl_pname_t) - 1,
							internal, sizeof (gcl_name_t),
							EP_B64_ENC_URL);
	}

	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_dbg_cprintf(Dbg, 2,
				"gdp_gcl_internal_name: ep_b64_decode failure\n"
				"\tstat = %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	else if (EP_STAT_TO_INT(estat) != sizeof (gcl_name_t))
	{
		ep_dbg_cprintf(Dbg, 2,
				"gdp_gcl_internal_name: ep_b64_decode length failure (%d != %zd)\n",
				EP_STAT_TO_INT(estat), sizeof (gcl_name_t));
		estat = EP_STAT_ABORT;
	}

	return estat;
}

/*
**	GDP_GCL_PARSENAME --- parse a (possibily human-friendly) GCL name
**
**		An externally printable version of an internal name must be
**		exactly GDP_GCL_PNAME_LEN (43) characters long and contain only
**		valid URL-Base64 characters.  These are base64 decoded.
**		All other names are considered human-friendly and are
**		sha256-encoded to get the internal name.
*/

EP_STAT
gdp_gcl_parse_name(const char *ext, gcl_name_t gcl_name)
{
	if (strlen(ext) == GDP_GCL_PNAME_LEN &&
			EP_STAT_ISOK(gdp_gcl_internal_name(ext, gcl_name)))
		return EP_STAT_OK;

	// must be human-oriented name
	SHA256((const uint8_t *) ext, strlen(ext), gcl_name);
	return EP_STAT_OK;
}

/*
**	GDP_GCL_NAME_IS_VALID --- test whether a GCL name is valid
**
**		Unfortunately, since SHA-256 is believed to be surjective
**		(that is, all values are possible), there is a slight
**		risk of a collision.
*/

bool
gdp_gcl_name_is_valid(const gcl_name_t gcl_name)
{
	const uint32_t *up;
	int i;

	up = (uint32_t *) gcl_name;
	for (i = 0; i < sizeof (gcl_name_t) / 4; i++)
		if (*up++ != 0)
			return true;
	return false;
}

/*
**  GDP_GCL_PRINT --- print a GCL (for debugging)
*/

void
gdp_gcl_print(
		const gdp_gcl_t *gcl,
		FILE *fp,
		int detail,
		int indent)
{
	if (detail > 0)
		fprintf(fp, "GCL@%p: ", gcl);
	if (gcl == NULL)
	{
		fprintf(fp, "NULL\n");
	}
	else
	{
		if (!gdp_gcl_name_is_valid(gcl->gcl_name))
		{
			fprintf(fp, "no name\n");
		}
		else
		{
			EP_ASSERT_POINTER_VALID(gcl);
			fprintf(fp, "%s\n", gcl->pname);
		}

		if (detail > 0)
		{
			fprintf(fp, "\tiomode = %d, refcnt = %d, reqs = %p\n",
					gcl->iomode, gcl->refcnt, LIST_FIRST(&gcl->reqs));
			if (detail > 1)
			{
				fprintf(fp, "\tfreefunc = %p, x = %p\n",
						gcl->freefunc, gcl->x);
			}
		}
	}
}



/*
**	GDP_INIT --- initialize this library
*/

EP_STAT
gdp_init(const char *gdpd_addr)
{
	static bool inited = false;
	EP_STAT estat;
	extern EP_STAT _gdp_do_init_1(void);
	extern EP_STAT _gdp_do_init_2(const char *);

	if (inited)
		return EP_STAT_OK;
	inited = true;

	// pass it on to the internal module
	estat = _gdp_do_init_1();
	EP_STAT_CHECK(estat, return estat);
	return _gdp_do_init_2(gdpd_addr);
}


/*
**	GDP_GCL_CREATE --- create a new GCL
*/

EP_STAT
gdp_gcl_create(gcl_name_t gcl_name,
				gdp_gclmd_t *gmd,
				gdp_gcl_t **pgcl)
{
	gdp_gcl_t *gcl = NULL;
	EP_STAT estat = EP_STAT_OK;

	// allocate the memory to hold the gcl_handle
	//		Note that ep_mem_* always returns, hence no test here
	estat = _gdp_gcl_newhandle(gcl_name, &gcl);
	EP_STAT_CHECK(estat, goto fail0);

	estat = _gdp_gcl_create(gcl, gmd, _GdpChannel, GDP_REQ_ALLOC_RID);
	EP_STAT_CHECK(estat, goto fail1);

	*pgcl = gcl;
	return estat;

fail1:
	if (gcl != NULL)
		_gdp_gcl_freehandle(gcl);

fail0:
	if (ep_dbg_test(Dbg, 8))
	{
		char ebuf[100];

		ep_dbg_printf("Could not create GCL: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	*pgcl = NULL;
	return estat;
}


/*
**	GDP_GCL_OPEN --- open a GCL for reading or further appending
*/

EP_STAT
gdp_gcl_open(gcl_name_t gcl_name,
			gdp_iomode_t mode,
			gdp_gcl_t **pgcl)
{
	EP_STAT estat;
	gdp_gcl_t *gcl = NULL;
	int cmd;

	if (mode == GDP_MODE_RO)
		cmd = GDP_CMD_OPEN_RO;
	else if (mode == GDP_MODE_AO)
		cmd = GDP_CMD_OPEN_AO;
	else
	{
		// illegal I/O mode
		ep_app_error("gdp_gcl_open: illegal mode %d", mode);
		return GDP_STAT_BAD_IOMODE;
	}

	if (!gdp_gcl_name_is_valid(gcl_name))
	{
		// illegal GCL name
		ep_dbg_cprintf(Dbg, 6, "gdp_gcl_open: null GCL name\n");
		return GDP_STAT_NULL_GCL;
	}

	estat = _gdp_gcl_newhandle(gcl_name, &gcl);
	EP_STAT_CHECK(estat, goto fail0);
	gcl->iomode = mode;

	estat = _gdp_gcl_open(gcl, cmd, _GdpChannel, GDP_REQ_ALLOC_RID);
	if (EP_STAT_ISOK(estat))
	{
		*pgcl = gcl;
	}
	else
	{
		_gdp_gcl_freehandle(gcl);
	}
fail0:
	return estat;
}


/*
**	GDP_GCL_CLOSE --- close an open GCL
*/

EP_STAT
gdp_gcl_close(gdp_gcl_t *gcl)
{
	return _gdp_gcl_close(gcl, _GdpChannel, 0);
}


/*
**	GDP_GCL_PUBLISH --- append a message to a writable GCL
*/

EP_STAT
gdp_gcl_publish(gdp_gcl_t *gcl, gdp_datum_t *datum)
{
	return _gdp_gcl_publish(gcl, datum, _GdpChannel, 0);
}


/*
**	GDP_GCL_READ --- read a message from a GCL
**
**		Parameters:
**			gcl --- the gcl from which to read
**			recno --- the record number to read
**			datum --- the message header (to avoid dynamic memory)
*/

EP_STAT
gdp_gcl_read(gdp_gcl_t *gcl,
			gdp_recno_t recno,
			gdp_datum_t *datum)
{
	EP_ASSERT_POINTER_VALID(datum);
	datum->recno = recno;

	return _gdp_gcl_read(gcl, datum, _GdpChannel, 0);
}


/*
**	GDP_GCL_SUBSCRIBE --- subscribe to a GCL
*/

EP_STAT
gdp_gcl_subscribe(gdp_gcl_t *gcl,
		gdp_recno_t start,
		int32_t numrecs,
		EP_TIME_SPEC *timeout,
		gdp_gcl_sub_cbfunc_t cbfunc,
		void *cbarg)
{
	return _gdp_gcl_subscribe(gcl, GDP_CMD_SUBSCRIBE, start, numrecs,
					timeout, cbfunc, cbarg, _GdpChannel, 0);
}


/*
**	GDP_GCL_MULTIREAD --- read multiple records from a GCL
**
**		Like gdp_gcl_subscribe, the data is returned through the event
**		interface.
*/

EP_STAT
gdp_gcl_multiread(gdp_gcl_t *gcl,
		gdp_recno_t start,
		int32_t numrecs,
		gdp_gcl_sub_cbfunc_t cbfunc,
		void *cbarg)
{
	return _gdp_gcl_subscribe(gcl, GDP_CMD_MULTIREAD, start, numrecs,
					NULL, cbfunc, cbarg, _GdpChannel, 0);
}

#if 0
EP_STAT
gdp_gcl_unsubscribe(gdp_gcl_t *gcl,
		void (*cbfunc)(gdp_gcl_t *, void *),
		void *arg)
{
	EP_ASSERT_POINTER_VALID(gcl);
	EP_ASSERT_POINTER_VALID(cbfunc);

	XXX;
}
#endif


EP_STAT
gdp_gcl_getmetadata(gdp_gcl_t *gcl,
		gdp_gclmd_t **gmdp)
{
	return _gdp_gcl_getmetadata(gcl, gmdp, _GdpChannel, 0);
}
