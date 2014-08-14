/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <gdp/gdp.h>
#include <gdp/gdp_log.h>
#include <gdp/gdp_stat.h>
#include <gdp/gdp_pkt.h>
#include <gdp/gdp_priv.h>
#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>
#include <ep/ep_prflags.h>
#include <ep/ep_string.h>
#include <ep/ep_thr.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
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
gdp_gcl_getname(const gcl_handle_t *gclh)
{
	return &gclh->gcl_name;
}

/*
**	GDP_GCL_MSG_PRINT --- print a message (for debugging)
*/

void
gdp_gcl_msg_print(const gdp_msg_t *msg,
			FILE *fp)
{
	unsigned char *d;
	int l;

	fprintf(fp, "GCL Record %d, ", msg->recno);
	if (msg->dbuf == NULL)
	{
		fprintf(fp, "no data");
		d = NULL;
		l = -1;
	}
	else
	{
		l = gdp_buf_getlength(msg->dbuf);
		fprintf(fp, "len %zd", l);
		d = gdp_buf_getptr(msg->dbuf, l);
	}

	if (msg->ts.stamp.tv_sec != TT_NOTIME)
	{
		fprintf(fp, ", timestamp ");
		tt_print_interval(&msg->ts, fp, true);
	}
	else
	{
		fprintf(fp, ", no timestamp");
	}

	if (l > 0)
	{
		fprintf(fp, "\n	 %s%.*s%s\n", EpChar->lquote, l, d, EpChar->rquote);
	}
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
	else if (EP_STAT_TO_LONG(estat) != 43)
	{
		ep_dbg_cprintf(Dbg, 2,
				"gdp_gcl_printable_name: ep_b64_encode length failure (%ld != 43)\n",
				EP_STAT_TO_LONG(estat));
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

	if (strlen(external) != GCL_PNAME_LEN)
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
	else if (EP_STAT_TO_LONG(estat) != sizeof (gcl_name_t))
	{
		ep_dbg_cprintf(Dbg, 2,
				"gdp_gcl_internal_name: ep_b64_decode length failure (%ld != %zd)\n",
				EP_STAT_TO_LONG(estat), sizeof (gcl_name_t));
		estat = EP_STAT_ABORT;
	}

	return estat;
}

/*
**	GDP_GCL_NAME_IS_ZERO --- test whether a GCL name is all zero
**
**		We assume that this means "no name".
*/

bool
gdp_gcl_name_is_zero(const gcl_name_t gcl_name)
{
	const uint32_t *up;
	int i;

	up = (uint32_t *) gcl_name;
	for (i = 0; i < sizeof (gcl_name_t) / 4; i++)
		if (*up++ != 0)
			return false;
	return true;
}

/*
**  GDP_GCL_PRINT --- print a GCL (for debugging)
*/

void
gdp_gcl_print(
		const gcl_handle_t *gclh,
		FILE *fp,
		int detail,
		int indent)
{
	gcl_pname_t nbuf;

	fprintf(fp, "GCL@%p: ", gclh);
	if (gclh == NULL)
	{
		fprintf(fp, "NULL\n");
	}
	else if (gdp_gcl_name_is_zero(gclh->gcl_name))
	{
		fprintf(fp, "no name\n");
	}
	else
	{
		EP_ASSERT_POINTER_VALID(gclh);

		gdp_gcl_printable_name(gclh->gcl_name, nbuf);
		fprintf(fp, "%s\n", nbuf);
	}
}



/*
**	GDP_INIT --- initialize this library
*/

EP_STAT
gdp_init(void)
{
	static bool inited = false;
	EP_STAT estat;
	extern EP_STAT _gdp_do_init_1(void);
	extern EP_STAT _gdp_do_init_2(void);

	if (inited)
		return EP_STAT_OK;
	inited = true;

	// pass it on to the internal module
	estat = _gdp_do_init_1();
	EP_STAT_CHECK(estat, return estat);
	return _gdp_do_init_2();
}


/*
**	GDP_GCL_CREATE --- create a new GCL
**
**		Right now we cheat.	 No network GCL need apply.
*/

EP_STAT
gdp_gcl_create(gcl_t *gcl_type,
				gcl_name_t gcl_name,
				gcl_handle_t **pgclh)
{
	gcl_handle_t *gclh = NULL;
	EP_STAT estat = EP_STAT_OK;

	// allocate the memory to hold the gcl_handle
	//		Note that ep_mem_* always returns, hence no test here
	estat = _gdp_gcl_newhandle(gcl_name, &gclh);
	EP_STAT_CHECK(estat, goto fail0);

	if (gcl_name == NULL || gdp_gcl_name_is_zero(gcl_name))
		_gdp_gcl_newname(gclh->gcl_name);

	estat = _gdp_invoke(GDP_CMD_CREATE, gclh, NULL);
	EP_STAT_CHECK(estat, goto fail1);

	// success
	*pgclh = gclh;
	return estat;

fail1:
	_gdp_gcl_freehandle(gclh);

fail0:
	if (ep_dbg_test(Dbg, 8))
	{
		char ebuf[100];

		ep_dbg_printf("Could not create GCL handle: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	*pgclh = NULL;
	return estat;
}


/*
**	GDP_GCL_OPEN --- open a GCL for reading or further appending
**
**		XXX: Should really specify whether we want to start reading:
**		(a) At the beginning of the log (easy).	 This includes random
**			access.
**		(b) Anything new that comes into the log after it is opened.
**			To do this we need to read the existing log to find the end.
*/

EP_STAT
gdp_gcl_open(gcl_name_t gcl_name,
			gdp_iomode_t mode,
			gcl_handle_t **pgclh)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_handle_t *gclh = NULL;
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

	if (gdp_gcl_name_is_zero(gcl_name))
	{
		// illegal GCL name
		ep_app_error("gdp_gcl_open: null GCL name");
		return GDP_STAT_NULL_GCL;
	}

	// XXX determine if name exists and is a GCL
	estat = _gdp_gcl_newhandle(gcl_name, &gclh);
	EP_STAT_CHECK(estat, goto fail1);
	gclh->iomode = mode;

	estat = _gdp_invoke(cmd, gclh, NULL);
	EP_STAT_CHECK(estat, goto fail1);

	// success!
	*pgclh = gclh;
	if (ep_dbg_test(Dbg, 10))
	{
		gcl_pname_t pname;

		gdp_gcl_printable_name(gclh->gcl_name, pname);
		ep_dbg_printf("Opened GCL %s\n", pname);
	}
	return estat;

fail1:
	estat = ep_stat_from_errno(errno);

	if (gclh != NULL)
		ep_mem_free(gclh);

	{
		gcl_pname_t pname;
		char ebuf[100];

		gdp_gcl_printable_name(gcl_name, pname);
		gdp_log(estat, "gdp_gcl_open: couldn't open GCL %s", pname);
		ep_dbg_cprintf(Dbg, 10,
				"Couldn't open GCL %s: %s\n",
				pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


/*
**	GDP_GCL_CLOSE --- close an open GCL
*/

EP_STAT
gdp_gcl_close(gcl_handle_t *gclh)
{
	EP_STAT estat;

	EP_ASSERT_POINTER_VALID(gclh);

	// tell the daemon to close it
	estat = _gdp_invoke(GDP_CMD_CLOSE, gclh, NULL);

	//XXX should probably check status

	// release resources held by this handle
	_gdp_gcl_freehandle(gclh);
	return estat;
}


/*
**	GDP_GCL_APPEND --- append a message to a writable GCL
*/

EP_STAT
gdp_gcl_append(gcl_handle_t *gclh,
			gdp_msg_t *msg)
{
	EP_STAT estat;

	EP_ASSERT_POINTER_VALID(gclh);

	if (msg->ts.stamp.tv_sec == TT_NOTIME)
		tt_now(&msg->ts);

	estat = _gdp_invoke(GDP_CMD_PUBLISH, gclh, msg);
	return estat;
}


/*
**	GDP_GCL_READ --- read a message from a GCL
**
**		Parameters:
**			gclh --- the gcl from which to read
**			recno --- the record number to read
**			reb --- the eventbuffer in which to store the results
**			msg --- the message header (to avoid dynamic memory)
*/

EP_STAT
gdp_gcl_read(gcl_handle_t *gclh,
			gdp_recno_t recno,
			gdp_buf_t *reb,
			gdp_msg_t *msg)
{
	EP_STAT estat = EP_STAT_OK;

	EP_ASSERT_POINTER_VALID(gclh);

	msg->recno = recno;
	msg->ts.stamp.tv_sec = TT_NOTIME;
	msg->dbuf = reb;
	estat = _gdp_invoke(GDP_CMD_READ, gclh, msg);

	// ok, done!
	return estat;
}

#if 0

/*
**	GDP_GCL_SUBSCRIBE --- subscribe to a GCL
*/

struct gdp_cb_arg
{
	struct event			*event;		// event triggering this callback
	gcl_handle_t			*gcl_handle; // gcl_handle triggering this callback
	gdp_gcl_sub_cbfunc_t	cbfunc;		// function to call
	void					*cbarg;		// argument to pass to cbfunc
	void					*buf;		// space to put the message
	size_t					bufsiz;		// size of buf
};


static void
gcl_sub_event_cb(evutil_socket_t fd,
		short what,
		void *cbarg)
{
	gdp_msg_t msg;
	EP_STAT estat;
	struct gdp_cb_arg *cba = cbarg;

	// get the next message from this gcl_handle
	estat = gdp_gcl_read(cba->gcl_handle, GCL_NEXT_MSG, &msg,
			cba->buf, cba->bufsiz);
	if (EP_STAT_ISOK(estat))
		(*cba->cbfunc)(cba->gcl_handle, &msg, cba->cbarg);
	//XXX;
}


EP_STAT
gdp_gcl_subscribe(gcl_handle_t *gclh,
		gdp_gcl_sub_cbfunc_t cbfunc,
		long offset,
		void *buf,
		size_t bufsiz,
		void *cbarg)
{
	EP_STAT estat = EP_STAT_OK;
	struct gdp_cb_arg *cba;
	struct timeval timeout = { 0, 100000 };		// 100 msec

	EP_ASSERT_POINTER_VALID(gclh);
	EP_ASSERT_POINTER_VALID(cbfunc);

	cba = ep_mem_zalloc(sizeof *cba);
	cba->gcl_handle = gclh;
	cba->cbfunc = cbfunc;
	cba->buf = buf;
	cba->bufsiz = bufsiz;
	cba->cbarg = cbarg;
	cba->event = event_new(GdpEventBase, fileno(gclh->fp),
			EV_READ|EV_PERSIST, &gcl_sub_event_cb, cba);
	event_add(cba->event, &timeout);
	//XXX;
	return estat;
}
#endif // 0

#if 0
EP_STAT
gdp_gcl_unsubscribe(gcl_handle_t *gclh,
		void (*cbfunc)(gcl_handle_t *, void *),
		void *arg)
{
	EP_ASSERT_POINTER_VALID(gclh);
	EP_ASSERT_POINTER_VALID(cbfunc);

	XXX;
}
#endif
