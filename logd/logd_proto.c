/* vim: set ai sw=4 sts=4 ts=4 : */

#include "logd.h"
#include "logd_physlog.h"
#include "logd_pubsub.h"

#include <gdp/gdp_gclmd.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdplogd.proto", "GDP Log Daemon protocol");

/*
**	GDPD_GCL_ERROR --- helper routine for returning errors
*/

EP_STAT
gdpd_gcl_error(gdp_name_t gcl_name, char *msg, EP_STAT logstat, int nak)
{
	gdp_pname_t pname;

	gdp_printable_name(gcl_name, pname);
	if (EP_STAT_ISSEVERE(logstat))
	{
		// server error (rather than client error)
		ep_log(logstat, "%s: %s", msg, pname);
	}
	return logstat;
}

void
flush_input_data(gdp_req_t *req, char *where)
{
	int i;

	if (req->pdu->datum != NULL &&
			(i = evbuffer_get_length(req->pdu->datum->dbuf)) > 0)
	{
		ep_dbg_cprintf(Dbg, 4,
				"flush_input_data: %s: flushing %d bytes of unexpected input\n",
				where, i);
		gdp_buf_reset(req->pdu->datum->dbuf);
	}
}

/*
**	Command implementations
*/

EP_STAT
implement_me(char *s)
{
	ep_app_error("Not implemented: %s", s);
	return GDP_STAT_NOT_IMPLEMENTED;
}


/***********************************************************************
**  GDP command implementations
**
**		Each of these takes a request as the argument.
**
**		These routines should set req->pdu->cmd to the "ACK" reply
**		code, which will be used if the command succeeds (i.e.,
**		returns EP_STAT_OK).  Otherwise the return status is decoded
**		to produce a NAK code.  A specific NAK code can be sent
**		using GDP_STAT_FROM_NAK(nak).
**
**		All routines are expected to consume all their input from
**		the channel and to write any output to the same channel.
**		They can consume any unexpected input using flush_input_data.
**
***********************************************************************/

/*
**  CMD_CREATE --- create new GCL.
*/

EP_STAT
cmd_create(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_gcl_t *gcl;
	gdp_gclmd_t *gmd;

	req->pdu->cmd = GDP_ACK_CREATED;

	// get the memory space
	estat = gcl_alloc(req->pdu->dst, GDP_MODE_AO, &gcl);
	EP_STAT_CHECK(estat, goto fail0);
	req->gcl = gcl;			// for debugging

	// collect metadata, if any
	gmd = _gdp_gclmd_deserialize(req->pdu->datum->dbuf);

	// no further input, so we can reset the buffer just to be safe
	flush_input_data(req, "cmd_create");

	// do the physical create
	estat = gcl_physcreate(gcl, gmd);
	gdp_gclmd_free(gmd);
	EP_STAT_CHECK(estat, goto fail1);

	// cache the open GCL Handle for possible future use
	EP_ASSERT_INSIST(gdp_name_is_valid(gcl->name));
	_gdp_gcl_cache_add(gcl, GDP_MODE_AO);

	// pass any creation info back to the caller
	// (none at this point)

	// release resources
	_gdp_gcl_decref(gcl);
	req->gcl = NULL;
	return estat;

fail1:
	_gdp_gcl_freehandle(gcl);

fail0:
	return estat;
}


/*
**  CMD_OPEN_RO, _AO --- open for read or append only
*/

EP_STAT
cmd_open_xx(gdp_req_t *req, gdp_iomode_t iomode)
{
	EP_STAT estat = EP_STAT_OK;

	req->pdu->cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_open_xx");

	// see if we already know about this GCL
	estat = get_open_handle(req, iomode);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_openxx: could not open GCL",
							estat, GDP_NAK_C_BADREQ);
	}

	req->pdu->datum->recno = gcl_max_recno(req->gcl);
	_gdp_gcl_decref(req->gcl);
	req->gcl = NULL;
	return estat;
}

EP_STAT
cmd_open_ao(gdp_req_t *req)
{
	return cmd_open_xx(req, GDP_MODE_AO);
}

EP_STAT
cmd_open_ro(gdp_req_t *req)
{
	return cmd_open_xx(req, GDP_MODE_RO);
}


/*
**  CMD_CLOSE --- close an open GCL
**
**		XXX	Since GCLs are shared between clients you really can't just
**			close things willy-nilly.  Thus, this is currently a no-op
**			until such time as reference counting works.
**
**		XXX	We need to have a way of expiring unused GCLs that are not
**			closed.
*/

EP_STAT
cmd_close(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;

	req->pdu->cmd = GDP_ACK_SUCCESS;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_close");

	// a bit wierd to open the GCL only to close it again....
	estat = get_open_handle(req, GDP_MODE_ANY);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_close: GCL not open",
							estat, GDP_NAK_C_BADREQ);
	}
	req->pdu->datum->recno = gcl_max_recno(req->gcl);
	_gdp_gcl_decref(req->gcl);
	req->gcl = NULL;

	return estat;
}


/*
**  CMD_READ --- read a single record from a GCL
**
**		This returns the data as part of the response.  To get multiple
**		values in one call, see cmd_subscribe.
*/

EP_STAT
cmd_read(gdp_req_t *req)
{
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_CONTENT;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_read");

	estat = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_read: GCL open failure",
							estat, GDP_NAK_C_BADREQ);
	}

	// handle record numbers relative to the end
	if (req->pdu->datum->recno <= 0)
	{
		req->pdu->datum->recno += gcl_max_recno(req->gcl) + 1;
		if (req->pdu->datum->recno <= 0)
		{
			// can't read before the beginning
			req->pdu->datum->recno = 1;
		}
	}

	gdp_buf_reset(req->pdu->datum->dbuf);
	estat = gcl_physread(req->gcl, req->pdu->datum);

	if (EP_STAT_IS_SAME(estat, EP_STAT_END_OF_FILE))
		estat = GDP_STAT_NAK_NOTFOUND;
	_gdp_gcl_decref(req->gcl);
	req->gcl = NULL;
	return estat;
}


/*
**  CMD_PUBLISH --- publish (write) a datum to a GCL
**
**		This will have side effects if there are subscriptions pending.
*/

EP_STAT
cmd_publish(gdp_req_t *req)
{
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_CREATED;

	estat = get_open_handle(req, GDP_MODE_AO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_publish: GCL not open",
							estat, GDP_NAK_C_BADREQ);
	}

	// make sure the timestamp is current
	estat = ep_time_now(&req->pdu->datum->ts);

	// create the message
	estat = gcl_physappend(req->gcl, req->pdu->datum);

	// send the new data to any subscribers
	if (EP_STAT_ISOK(estat))
		sub_notify_all_subscribers(req);

	// we can now let the data in the request go
	evbuffer_drain(req->pdu->datum->dbuf,
			evbuffer_get_length(req->pdu->datum->dbuf));

	// we're no longer using this handle
	_gdp_gcl_decref(req->gcl);
	req->gcl = NULL;

	return estat;
}


/*
**  POST_SUBSCRIBE --- do subscription work after initial ACK
**
**		Assuming the subscribe worked we are now going to deliver any
**		previously existing records.  Once those are all sent we can
**		convert this to an ordinary subscription.  If the subscribe
**		request is satisified, we remove it.
*/

void
post_subscribe(gdp_req_t *req)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 38,
			"post_subscribe: numrecs = %d, recno = %"PRIgdp_recno"\n",
			req->numrecs, req->pdu->datum->recno);

	// make sure the request has the right command
	req->pdu->cmd = GDP_ACK_CONTENT;

	while (req->numrecs >= 0)
	{
		// see if data pre-exists in the GCL
		if (req->pdu->datum->recno > gcl_max_recno(req->gcl))
		{
			// no, it doesn't; convert to long-term subscription
			break;
		}

		// get the next record and return it as an event
		estat = gcl_physread(req->gcl, req->pdu->datum);
		if (EP_STAT_ISOK(estat))
		{
			// OK, the next record exists: send it
			req->stat = estat = _gdp_pdu_out(req->pdu, req->chan);

			// have to clear the old data
			evbuffer_drain(req->pdu->datum->dbuf,
					evbuffer_get_length(req->pdu->datum->dbuf));

			// advance to the next record
			if (req->numrecs > 0 && --req->numrecs == 0)
			{
				// numrecs was positive, now zero, but zero means infinity
				req->numrecs--;
			}
			req->pdu->datum->recno++;
		}
		else if (!EP_STAT_IS_SAME(estat, EP_STAT_END_OF_FILE))
		{
			// this is some error that should be logged
			ep_log(estat, "post_subscribe: bad read");
			req->numrecs = -1;		// terminate subscription
		}
		else
		{
			// shouldn't happen
			ep_log(estat, "post_subscribe: read EOF");
		}

		// if we didn't successfully send a record, terminate
		EP_STAT_CHECK(estat, break);
	}

	if (req->numrecs < 0 || !EP_UT_BITSET(GDP_REQ_SUBUPGRADE, req->flags))
	{
		// no more to read: do cleanup & send termination notice
		sub_end_subscription(req);
	}
	else
	{
		ep_dbg_cprintf(Dbg, 24, "post_subscribe: converting to subscription\n");
		req->flags |= GDP_REQ_SUBSCRIPTION;

		// link this request into the GCL so the subscription can be found
		ep_thr_mutex_lock(&req->gcl->mutex);
		EP_ASSERT(!req->ongcllist);
		LIST_INSERT_HEAD(&req->gcl->reqs, req, gcllist);
		req->ongcllist = true;
		ep_thr_mutex_unlock(&req->gcl->mutex);
	}
}


/*
**  CMD_SUBSCRIBE --- subscribe command
**
**		Arranges to return existing data (if any) after the response
**		is sent, and non-existing data (if any) as a side-effect of
**		publish.
**
**		XXX	Race Condition: if records are written between the time
**			the subscription and the completion of the first half of
**			this process, some records may be lost.  For example,
**			if the GCL has 20 records (1-20) and you ask for 20
**			records starting at record 11, you probably want records
**			11-30.  But if during the return of records 11-20 another
**			record (21) is written, then the second half of the
**			subscription will actually return records 22-31.
**
**		XXX	Does not implement timeouts.
*/

EP_STAT
cmd_subscribe(gdp_req_t *req)
{
	EP_STAT estat;
	EP_TIME_SPEC timeout;

	req->pdu->cmd = GDP_ACK_SUCCESS;

	// find the GCL handle
	estat = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_subscribe: GCL not open",
							estat, GDP_NAK_C_BADREQ);
	}

	// get the additional parameters: number of records and timeout
	req->numrecs = (int) gdp_buf_get_uint32(req->pdu->datum->dbuf);
	gdp_buf_get_timespec(req->pdu->datum->dbuf, &timeout);

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_subscribe: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				req->pdu->datum->recno, req->numrecs);
		gdp_gcl_print(req->gcl, ep_dbg_getfile(), 1, 0);
	}

	// should have no more input data; ignore anything there
	flush_input_data(req, "cmd_subscribe");

	if (req->numrecs < 0)
	{
		req->pdu->cmd = GDP_NAK_C_BADOPT;
		return GDP_STAT_FROM_C_NAK(req->pdu->cmd);
	}

	// mark this as persistent and upgradable
	req->flags |= GDP_REQ_PERSIST | GDP_REQ_SUBUPGRADE;

	// get our starting point, which may be relative to the end
	if (req->pdu->datum->recno <= 0)
	{
		req->pdu->datum->recno += gcl_max_recno(req->gcl) + 1;
		if (req->pdu->datum->recno <= 0)
		{
			// still starts before beginning; start from beginning
			req->pdu->datum->recno = 1;
		}
	}

	ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: starting from %" PRIgdp_recno
			", %d records\n",
			req->pdu->datum->recno, req->numrecs);

	// if some of the records already exist, arrange to return them
	if (req->pdu->datum->recno <= gcl_max_recno(req->gcl))
	{
		ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: doing post processing\n");
		req->cb.gdpd = &post_subscribe;
		req->postproc = true;
	}
	else
	{
		// this is a pure "future" subscription
		ep_dbg_cprintf(Dbg, 24, "cmd_subscribe: enabling subscription\n");
		req->flags |= GDP_REQ_SUBSCRIPTION;

		// link this request into the GCL so the subscription can be found
		ep_thr_mutex_lock(&req->gcl->mutex);
		EP_ASSERT(!req->ongcllist);
		LIST_INSERT_HEAD(&req->gcl->reqs, req, gcllist);
		req->ongcllist = true;
		ep_thr_mutex_unlock(&req->gcl->mutex);
	}

	// we don't drop the GCL reference until the subscription is satisified

	return EP_STAT_OK;
}


/*
**  CMD_MULTIREAD --- read multiple records
**
**		Arranges to return existing data (if any) after the response
**		is sent.  No long-term subscription will ever be created, but
**		much of the infrastructure is reused.
*/

EP_STAT
cmd_multiread(gdp_req_t *req)
{
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_SUCCESS;

	// find the GCL handle
	estat  = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_multiread: GCL not open",
							estat, GDP_NAK_C_BADREQ);
	}

	// get the additional parameters: number of records and timeout
	req->numrecs = (int) gdp_buf_get_uint32(req->pdu->datum->dbuf);

	if (ep_dbg_test(Dbg, 14))
	{
		ep_dbg_printf("cmd_multiread: first = %" PRIgdp_recno ", numrecs = %d\n  ",
				req->pdu->datum->recno, req->numrecs);
		gdp_gcl_print(req->gcl, ep_dbg_getfile(), 1, 0);
	}

	// should have no more input data; ignore anything there
	flush_input_data(req, "cmd_multiread");

	// get our starting point, which may be relative to the end
	if (req->pdu->datum->recno <= 0)
	{
		req->pdu->datum->recno += gcl_max_recno(req->gcl) + 1;
		if (req->pdu->datum->recno <= 0)
		{
			// still starts before beginning; start from beginning
			req->pdu->datum->recno = 1;
		}
	}

	if (req->numrecs < 0)
	{
		req->pdu->cmd = GDP_NAK_C_BADOPT;
		return GDP_STAT_FROM_C_NAK(req->pdu->cmd);
	}

	// get our starting point, which may be relative to the end
	if (req->pdu->datum->recno <= 0)
	{
		req->pdu->datum->recno += gcl_max_recno(req->gcl) + 1;
		if (req->pdu->datum->recno <= 0)
		{
			// still starts before beginning; start from beginning
			req->pdu->datum->recno = 1;
		}
	}

	ep_dbg_cprintf(Dbg, 24, "cmd_multiread: starting from %" PRIgdp_recno
			", %d records\n",
			req->pdu->datum->recno, req->numrecs);

	// if some of the records already exist, arrange to return them
	if (req->pdu->datum->recno <= gcl_max_recno(req->gcl))
	{
		ep_dbg_cprintf(Dbg, 24, "cmd_multiread: doing post processing\n");
		req->cb.gdpd = &post_subscribe;
		req->postproc = true;

		// make this a "snapshot", i.e., don't read additional records
		int32_t nrec = gcl_max_recno(req->gcl) - req->pdu->datum->recno;
		if (nrec < req->numrecs || req->numrecs == 0)
			req->numrecs = nrec + 1;

		// keep the request around until the post-processing is done
		req->flags |= GDP_REQ_PERSIST;
	}
	else
	{
		// this is a pure "future" subscription
		ep_dbg_cprintf(Dbg, 24, "cmd_multiread: enabling subscription\n");
		req->flags |= GDP_REQ_SUBSCRIPTION;

		// link this request into the GCL so the subscription can be found
		ep_thr_mutex_lock(&req->gcl->mutex);
		EP_ASSERT(!req->ongcllist);
		LIST_INSERT_HEAD(&req->gcl->reqs, req, gcllist);
		req->ongcllist = true;
		ep_thr_mutex_unlock(&req->gcl->mutex);
	}

	return EP_STAT_OK;
}


/*
**  CMD_UNSUBSCRIBE --- terminate a subscription
**
**		XXX not implemented yet
*/


/*
**  CMD_GETMETADATA --- get metadata for a GCL
*/

EP_STAT
cmd_getmetadata(gdp_req_t *req)
{
	gdp_gclmd_t *gmd;
	EP_STAT estat;

	req->pdu->cmd = GDP_ACK_CONTENT;

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_getmetadata");

	estat = get_open_handle(req, GDP_MODE_RO);
	if (!EP_STAT_ISOK(estat))
	{
		return gdpd_gcl_error(req->pdu->dst, "cmd_read: GCL open failure",
							estat, GDP_NAK_C_BADREQ);
	}

	// get the metadata into memory
	estat = gcl_physgetmetadata(req->gcl, &gmd);
	EP_STAT_CHECK(estat, goto fail0);

	// serialize it to the client
	_gdp_gclmd_serialize(gmd, req->pdu->datum->dbuf);

fail0:
	_gdp_gcl_decref(req->gcl);
	req->gcl = NULL;
	return estat;
}


/*
**  CMD_NOT_IMPLEMENTED --- generic "not implemented" error
*/

EP_STAT
cmd_not_implemented(gdp_req_t *req)
{
	//XXX print/log something here?

	// should have no input data; ignore anything there
	flush_input_data(req, "cmd_not_implemented");

	req->pdu->cmd = GDP_NAK_S_NOTIMPL;
	_gdp_pdu_out_hard(req->pdu, req->chan);
	return GDP_STAT_NAK_NOTIMPL;
}


/**************** END OF COMMAND IMPLEMENTATIONS ****************/


/*
**	DISPATCH_CMD --- dispatch a command via the DispatchTable
**
**	Parameters:
**		req --- the request to be satisified
**		conn --- the connection (socket) handle
*/

EP_STAT
dispatch_cmd(gdp_req_t *req)
{
	EP_STAT estat;
	int cmd = req->pdu->cmd;

	if (ep_dbg_test(Dbg, 18))
	{
		ep_dbg_printf("dispatch_cmd: >>> command %d (%s)\n",
				cmd, _gdp_proto_cmd_name(cmd));
		if (ep_dbg_test(Dbg, 30))
			_gdp_req_dump(req, ep_dbg_getfile());
	}

	estat = _gdp_req_dispatch(req);

	// swap source and destination addresses
	{
		gdp_name_t temp;

		memcpy(temp, req->pdu->src, sizeof temp);
		memcpy(req->pdu->src, req->pdu->dst, sizeof req->pdu->src);
		memcpy(req->pdu->dst, temp, sizeof req->pdu->dst);
	}

	// decode return status as an ACK/NAK command
	if (!EP_STAT_ISOK(estat))
	{
		req->pdu->cmd = GDP_NAK_S_INTERNAL;
		if (EP_STAT_REGISTRY(estat) == EP_REGISTRY_UCB &&
			EP_STAT_MODULE(estat) == GDP_MODULE)
		{
			int d = EP_STAT_DETAIL(estat);

			if (d >= 400 && d < (400 + GDP_NAK_C_MAX - GDP_NAK_C_MIN))
				req->pdu->cmd = d - 400 + GDP_NAK_C_MIN;
			else if (d >= 500 && d < (500 + GDP_NAK_S_MAX - GDP_NAK_S_MIN))
				req->pdu->cmd = d - 500 + GDP_NAK_S_MIN;
			else
				req->pdu->cmd = GDP_NAK_S_INTERNAL;
		}
	}

	if (ep_dbg_test(Dbg, 18))
	{
		char ebuf[200];

		ep_dbg_printf("dispatch_cmd: <<< %s, status %s (%s)\n",
				_gdp_proto_cmd_name(cmd), _gdp_proto_cmd_name(req->pdu->cmd),
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(Dbg, 30))
			_gdp_pdu_dump(req->pdu, ep_dbg_getfile());
	}
	return estat;
}


/*
**  GDPD_PROTO_INIT --- initialize protocol module
*/

static struct cmdfuncs	CmdFuncs[] =
{
	{ GDP_CMD_CREATE,		cmd_create		},
	{ GDP_CMD_OPEN_AO,		cmd_open_ao		},
	{ GDP_CMD_OPEN_RO,		cmd_open_ro		},
	{ GDP_CMD_CLOSE,		cmd_close		},
	{ GDP_CMD_READ,			cmd_read		},
	{ GDP_CMD_PUBLISH,		cmd_publish		},
	{ GDP_CMD_SUBSCRIBE,	cmd_subscribe	},
	{ GDP_CMD_MULTIREAD,	cmd_multiread	},
	{ GDP_CMD_GETMETADATA,	cmd_getmetadata	},
	{ 0,					NULL			}
};

void
gdpd_proto_init(void)
{
	// register the commands we implement
	_gdp_register_cmdfuncs(CmdFuncs);
}