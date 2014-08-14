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
**	This implements the GDP Protocol.
**
**	In the future this may need to be extended to have knowledge of
**	TSN/AVB, but for now we don't worry about that.
*/

/************************  PRIVATE	************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.proto", "GDP protocol processing");


//XXX this really shouldn't be global: restricts us to one port
static gdp_chan_t	*GdpChannel;	// our primary protocol port


/***********************************************************************
**
**	Request ID handling
**
**		Very simplistic for now.  RIDs really only need to be unique
**		within a given GCL.
*/

static gdp_rid_t	MaxRid = 0;

gdp_rid_t
_gdp_rid_new(gcl_handle_t *gclh)
{
	return ++MaxRid;
}

char *
gdp_rid_tostr(gdp_rid_t rid, char *buf, size_t len)
{
	snprintf(buf, len, "%d", rid);
	return buf;
}


/***********************************************************************
**
**  GDP Request handling
*/

// unused request structures
static struct req_head	ReqFreeList = LIST_HEAD_INITIALIZER(ReqFreeList);
static EP_THR_MUTEX		ReqFreeListMutex	EP_THR_MUTEX_INITIALIZER;

/*
**  _GDP_REQ_NEW --- allocate a new request
**
**	Parameters:
**		cmd --- the command to be issued
**		gclh --- the associated GCL handle
**		reqp --- a pointer to the output area
**
**	Returns:
**		status
**		The request has been allocated an id (possibly unique to gclh),
**			but the request has not been linked onto the GCL's request list.
**			This allows the caller to adjust the request without locking it.
*/

EP_STAT
_gdp_req_new(int cmd,
		gcl_handle_t *gclh,
		uint32_t flags,
		gdp_req_t **reqp)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;

	// get memory, off free list if possible
	ep_thr_mutex_lock(&ReqFreeListMutex);
	if ((req = LIST_FIRST(&ReqFreeList)) != NULL)
	{
		LIST_REMOVE(req, list);
	}
	ep_thr_mutex_unlock(&ReqFreeListMutex);
	if (req == NULL)
	{
		req = ep_mem_zalloc(sizeof *req);
		ep_thr_mutex_init(&req->mutex);
		ep_thr_cond_init(&req->cond);
		req->pkt.dbuf = gdp_buf_new();
		gdp_buf_setlock(req->pkt.dbuf, &req->mutex);
	}

	req->gclh = gclh;
	req->stat = EP_STAT_OK;
	req->pkt.cmd = cmd;
	req->pkt.recno = GDP_PKT_NO_RECNO;
	req->pkt.ts.stamp.tv_sec = TT_NOTIME;
	req->pkt.dlen = 0;
	if (gclh != NULL)
		memcpy(req->pkt.gcl_name, gclh->gcl_name, sizeof req->pkt.gcl_name);
	if (gclh == NULL || EP_UT_BITSET(GDP_REQ_SYNC, flags))
	{
		// just use constant zero; any value would be fine
		req->pkt.rid = GDP_PKT_NO_RID;
	}
	else
	{
		// allocate a new unique request id
		req->pkt.rid = _gdp_rid_new(gclh);
	}

	// success
	*reqp = req;
	return estat;
}


void
_gdp_req_free(gdp_req_t *req)
{
	ep_thr_mutex_lock(&req->mutex);

	// remove it from the old list
	if (req->gclh != NULL)
	{
		ep_thr_mutex_lock(&req->gclh->mutex);
		LIST_REMOVE(req, list);
		ep_thr_mutex_unlock(&req->gclh->mutex);
	}

	// add it to the free list
	ep_thr_mutex_lock(&ReqFreeListMutex);
	LIST_INSERT_HEAD(&ReqFreeList, req, list);
	ep_thr_mutex_unlock(&ReqFreeListMutex);

	ep_thr_mutex_unlock(&req->mutex);
}

void
_gdp_req_freeall(struct req_head *reqlist)
{
	gdp_req_t *r1 = LIST_FIRST(reqlist);

	while (r1 != NULL)
	{
		gdp_req_t *r2 = LIST_NEXT(r1, list);
		_gdp_req_free(r1);
		r1 = r2;
	}
}


gdp_req_t *
_gdp_req_find(gcl_handle_t *gclh, gdp_rid_t rid)
{
	gdp_req_t *req;
	gdp_req_t *rcursor;

	ep_thr_mutex_lock(&gclh->mutex);
	LIST_FOREACH_SAFE(req, &gclh->reqs, list, rcursor)
	{
		if (req->pkt.rid != rid)
			continue;
		if (!EP_UT_BITSET(GDP_REQ_PERSIST, req->flags))
			LIST_REMOVE(req, list);
		break;
	}
	ep_thr_mutex_unlock(&gclh->mutex);
	return req;
}


/*
**   GDP_REQ_SEND --- send a request to the GDP daemon
**
**		This makes no attempt to read results.
*/

EP_STAT
_gdp_req_send(gdp_req_t *req, gdp_msg_t *msg)
{
	EP_STAT estat;
	gcl_handle_t *gclh = req->gclh;

	ep_dbg_cprintf(Dbg, 45, "gdp_req_send: cmd=%d (%s)\n",
			req->pkt.cmd, _gdp_proto_cmd_name(req->pkt.cmd));
	EP_ASSERT(gclh != NULL);

	if (msg != NULL)
	{
		EP_ASSERT(msg->dbuf != NULL);
		EP_ASSERT(req->pkt.dbuf != NULL);
		req->pkt.recno = msg->recno;
		req->pkt.ts = msg->ts;
		gdp_buf_copy(msg->dbuf, req->pkt.dbuf);
	}

	// register this handle so we can process the results
	//		(it's likely that it's already in the cache)
	_gdp_gcl_cache_add(gclh, 0);

	// link the request to the GCL
	ep_thr_mutex_lock(&gclh->mutex);
	LIST_INSERT_HEAD(&gclh->reqs, req, list);
	ep_thr_mutex_unlock(&gclh->mutex);

	// write the message out
	estat = _gdp_pkt_out(&req->pkt, bufferevent_get_output(GdpChannel));
	return estat;
}



/*
**	GDP_INVOKE --- do a remote invocation to the GDP daemon
**
**		This acts as an remote procedure call.  In particular it
**		waits to get a result, which makes it inappropriate for
**		instances where multiple commands are in flight, or
**		where a command can return multiple values (e.g., subscribe).
*/

EP_STAT
_gdp_invoke(int cmd, gcl_handle_t *gclh, gdp_msg_t *msg)
{
	EP_STAT estat;
	gdp_req_t *req;

	EP_ASSERT_POINTER_VALID(gclh);
	ep_dbg_cprintf(Dbg, 43, "gdp_invoke: cmd=%d (%s)\n",
			cmd, _gdp_proto_cmd_name(cmd));

	/*
	**  Top Half: sending the command
	*/

	// get an associated request
	estat = _gdp_req_new(cmd, gclh, GDP_REQ_SYNC, &req);
	EP_STAT_CHECK(estat, goto fail0);

	req->pkt.cmd = cmd;

	estat = _gdp_req_send(req, msg);
	EP_STAT_CHECK(estat, goto fail0);

	/*
	**  Bottom Half: read the response
	*/

	// wait until we receive a result
	//		XXX need a timeout here
	ep_dbg_cprintf(Dbg, 37, "gdp_invoke: waiting\n");
	ep_thr_mutex_lock(&req->mutex);
	while (!EP_UT_BITSET(GDP_REQ_DONE, req->flags))
	{
		ep_thr_cond_wait(&req->cond, &req->mutex);
	}
	// mutex is released below

	//XXX what status will/should we return?
	if (event_base_got_exit(GdpIoEventBase))
	{
		ep_dbg_cprintf(Dbg, 1, "gdp_invoke: exiting on loopexit\n");
		estat = GDP_STAT_INTERNAL_ERROR;
	}

	if (EP_UT_BITSET(GDP_REQ_DONE, req->flags))
	{
		estat = req->stat;
		if (msg != NULL)
		{
			msg->recno = req->pkt.recno;
			msg->ts = req->pkt.ts;
		}
	}

	// ok, done!
	ep_thr_mutex_unlock(&req->mutex);
	_gdp_req_free(req);

fail0:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 12, "gdp_invoke: returning %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}



/***********************************************************************
**
**	Protocol processing (ACK/NAK)
**
**		All of these take as parameters:
**			req --- the request information (including packet header)
**			chan --- the libevent channel from which this pdu came
**
**		They can return GDP_STAT_KEEP_READING to tell the upper
**		layer that the whole packet hasn't been read yet.
**
***********************************************************************/

static EP_STAT
ack_success(gdp_req_t *req,
		gdp_chan_t *chan)
{
	EP_STAT estat;
	size_t tocopy;
	gcl_handle_t *gclh;

	// we require a request
	if (req == NULL)
	{
		estat = GDP_STAT_PROTOCOL_FAIL;
		gdp_log(estat, "ack_success: null request");
		goto fail0;
	}

	estat = GDP_STAT_FROM_ACK(req->pkt.cmd);
	tocopy = req->pkt.dlen;
	gclh = req->gclh;

	//	If we started with no gcl id, adopt from incoming packet.
	//	This can happen when creating a GCL.
	if (gclh != NULL)
	{
		if (gdp_gcl_name_is_zero(gclh->gcl_name))
			memcpy(gclh->gcl_name, req->pkt.gcl_name, sizeof gclh->gcl_name);

		while (tocopy > 0)
		{
			int i;

			if (req->pkt.dbuf != NULL)
				i = evbuffer_remove_buffer(bufferevent_get_input(chan),
						req->pkt.dbuf, tocopy);
			else
				i = gdp_buf_drain(bufferevent_get_input(chan), tocopy);
			if (i <= 0)
			{
				ep_dbg_cprintf(Dbg, 4,
						"ack_success: data copy failure: %s\n",
						strerror(errno));
				break;
			}
			tocopy -= i;
		}
	}

fail0:
	return estat;
}


static EP_STAT
nak(int cmd, const char *where)
{
	ep_dbg_cprintf(Dbg, 2, "nak_%s: received %d\n", where, cmd);
	return GDP_STAT_FROM_NAK(cmd);
}


static EP_STAT
nak_client(gdp_req_t *req,
		gdp_chan_t *chan)
{
	return nak(req->pkt.cmd, "client");
}


static EP_STAT
nak_server(gdp_req_t *req,
		gdp_chan_t *chan)
{
	return nak(req->pkt.cmd, "server");
}



/*
**	Command/Ack/Nak Dispatch Table
*/

typedef struct
{
	cmdfunc_t	*func;		// function to call
	const char	*name;		// name of command (for debugging)
} dispatch_ent_t;

#define NOENT		{ NULL, NULL }

static dispatch_ent_t	DispatchTable[256] =
{
	{ NULL,				"CMD_KEEPALIVE" },			// 0
	NOENT,				// 1
	NOENT,				// 2
	NOENT,				// 3
	NOENT,				// 4
	NOENT,				// 5
	NOENT,				// 6
	NOENT,				// 7
	NOENT,				// 8
	NOENT,				// 9
	NOENT,				// 10
	NOENT,				// 11
	NOENT,				// 12
	NOENT,				// 13
	NOENT,				// 14
	NOENT,				// 15
	NOENT,				// 16
	NOENT,				// 17
	NOENT,				// 18
	NOENT,				// 19
	NOENT,				// 20
	NOENT,				// 21
	NOENT,				// 22
	NOENT,				// 23
	NOENT,				// 24
	NOENT,				// 25
	NOENT,				// 26
	NOENT,				// 27
	NOENT,				// 28
	NOENT,				// 29
	NOENT,				// 30
	NOENT,				// 31
	NOENT,				// 32
	NOENT,				// 33
	NOENT,				// 34
	NOENT,				// 35
	NOENT,				// 36
	NOENT,				// 37
	NOENT,				// 38
	NOENT,				// 39
	NOENT,				// 40
	NOENT,				// 41
	NOENT,				// 42
	NOENT,				// 43
	NOENT,				// 44
	NOENT,				// 45
	NOENT,				// 46
	NOENT,				// 47
	NOENT,				// 48
	NOENT,				// 49
	NOENT,				// 50
	NOENT,				// 51
	NOENT,				// 52
	NOENT,				// 53
	NOENT,				// 54
	NOENT,				// 55
	NOENT,				// 56
	NOENT,				// 57
	NOENT,				// 58
	NOENT,				// 59
	NOENT,				// 60
	NOENT,				// 61
	NOENT,				// 62
	NOENT,				// 63
	{ NULL,				"CMD_PING"				},			// 64
	{ NULL,				"CMD_HELLO"				},			// 65
	{ NULL,				"CMD_CREATE"			},			// 66
	{ NULL,				"CMD_OPEN_AO"			},			// 67
	{ NULL,				"CMD_OPEN_RO"			},			// 68
	{ NULL,				"CMD_CLOSE"				},			// 69
	{ NULL,				"CMD_READ"				},			// 70
	{ NULL,				"CMD_PUBLISH"			},			// 71
	{ NULL,				"CMD_SUBSCRIBE"			},			// 72
	NOENT,				// 73
	NOENT,				// 74
	NOENT,				// 75
	NOENT,				// 76
	NOENT,				// 77
	NOENT,				// 78
	NOENT,				// 79
	NOENT,				// 80
	NOENT,				// 81
	NOENT,				// 82
	NOENT,				// 83
	NOENT,				// 84
	NOENT,				// 85
	NOENT,				// 86
	NOENT,				// 87
	NOENT,				// 88
	NOENT,				// 89
	NOENT,				// 90
	NOENT,				// 91
	NOENT,				// 92
	NOENT,				// 93
	NOENT,				// 94
	NOENT,				// 95
	NOENT,				// 96
	NOENT,				// 97
	NOENT,				// 98
	NOENT,				// 99
	NOENT,				// 100
	NOENT,				// 101
	NOENT,				// 102
	NOENT,				// 103
	NOENT,				// 104
	NOENT,				// 105
	NOENT,				// 106
	NOENT,				// 107
	NOENT,				// 108
	NOENT,				// 109
	NOENT,				// 110
	NOENT,				// 111
	NOENT,				// 112
	NOENT,				// 113
	NOENT,				// 114
	NOENT,				// 115
	NOENT,				// 116
	NOENT,				// 117
	NOENT,				// 118
	NOENT,				// 119
	NOENT,				// 120
	NOENT,				// 121
	NOENT,				// 122
	NOENT,				// 123
	NOENT,				// 124
	NOENT,				// 125
	NOENT,				// 126
	NOENT,				// 127
	{ ack_success,		"ACK_SUCCESS"			},			// 128
	{ ack_success,		"ACK_DATA_CREATED"		},			// 129
	{ ack_success,		"ACK_DATA_DEL"			},			// 130
	{ ack_success,		"ACK_DATA_VALID"		},			// 131
	{ ack_success,		"ACK_DATA_CHANGED"		},			// 132
	{ ack_success,		"ACK_DATA_CONTENT"		},			// 133
	NOENT,				// 134
	NOENT,				// 135
	NOENT,				// 136
	NOENT,				// 137
	NOENT,				// 138
	NOENT,				// 139
	NOENT,				// 140
	NOENT,				// 141
	NOENT,				// 142
	NOENT,				// 143
	NOENT,				// 144
	NOENT,				// 145
	NOENT,				// 146
	NOENT,				// 147
	NOENT,				// 148
	NOENT,				// 149
	NOENT,				// 150
	NOENT,				// 151
	NOENT,				// 152
	NOENT,				// 153
	NOENT,				// 154
	NOENT,				// 155
	NOENT,				// 156
	NOENT,				// 157
	NOENT,				// 158
	NOENT,				// 159
	NOENT,				// 160
	NOENT,				// 161
	NOENT,				// 162
	NOENT,				// 163
	NOENT,				// 164
	NOENT,				// 165
	NOENT,				// 166
	NOENT,				// 167
	NOENT,				// 168
	NOENT,				// 169
	NOENT,				// 170
	NOENT,				// 171
	NOENT,				// 172
	NOENT,				// 173
	NOENT,				// 174
	NOENT,				// 175
	NOENT,				// 176
	NOENT,				// 177
	NOENT,				// 178
	NOENT,				// 179
	NOENT,				// 180
	NOENT,				// 181
	NOENT,				// 182
	NOENT,				// 183
	NOENT,				// 184
	NOENT,				// 185
	NOENT,				// 186
	NOENT,				// 187
	NOENT,				// 188
	NOENT,				// 189
	NOENT,				// 190
	NOENT,				// 191
	{ nak_client,		"NAK_C_BADREQ"			},			// 192
	{ nak_client,		"NAK_C_UNAUTH"			},			// 193
	{ nak_client,		"NAK_C_BADOPT"			},			// 194
	{ nak_client,		"NAK_C_FORBIDDEN"		},			// 195
	{ nak_client,		"NAK_C_NOTFOUND"		},			// 196
	{ nak_client,		"NAK_C_METHNOTALLOWED"	},			// 197
	{ nak_client,		"NAK_C_NOTACCEPTABLE"	},			// 198
	NOENT,				// 199
	NOENT,				// 200
	NOENT,				// 201
	NOENT,				// 202
	NOENT,				// 203
	{ nak_client,		"NAK_C_PRECONFAILED"	},			// 204
	{ nak_client,		"NAK_C_TOOLARGE"		},			// 205
	NOENT,				// 206
	{ nak_client,		"NAK_C_UNSUPMEDIA"		},			// 207
	NOENT,				// 208
	NOENT,				// 209
	NOENT,				// 210
	NOENT,				// 211
	NOENT,				// 212
	NOENT,				// 213
	NOENT,				// 214
	NOENT,				// 215
	NOENT,				// 216
	NOENT,				// 217
	NOENT,				// 218
	NOENT,				// 219
	NOENT,				// 220
	NOENT,				// 221
	NOENT,				// 222
	NOENT,				// 223
	{ nak_server,		"NAK_S_INTERNAL"		},			// 224
	{ nak_server,		"NAK_S_NOTIMPL"			},			// 225
	{ nak_server,		"NAK_S_BADGATEWAY"		},			// 226
	{ nak_server,		"NAK_S_SVCUNAVAIL"		},			// 227
	{ nak_server,		"NAK_S_GWTIMEOUT"		},			// 228
	{ nak_server,		"NAK_S_PROXYNOTSUP"		},			// 229
	NOENT,				// 230
	NOENT,				// 231
	NOENT,				// 232
	NOENT,				// 233
	NOENT,				// 234
	NOENT,				// 235
	NOENT,				// 236
	NOENT,				// 237
	NOENT,				// 238
	NOENT,				// 239
	NOENT,				// 240
	NOENT,				// 241
	NOENT,				// 242
	NOENT,				// 243
	NOENT,				// 244
	NOENT,				// 245
	NOENT,				// 246
	NOENT,				// 247
	NOENT,				// 248
	NOENT,				// 249
	NOENT,				// 250
	NOENT,				// 251
	NOENT,				// 252
	NOENT,				// 253
	NOENT,				// 254
	NOENT,				// 255
};


/*
**	_GDP_CMD_NAME --- return name of command
*/

const char *
_gdp_proto_cmd_name(uint8_t cmd)
{
	dispatch_ent_t *d = &DispatchTable[cmd];

	if (d->name == NULL)
		return "???";
	else
		return d->name;
}


void
_gdp_register_cmdfuncs(struct cmdfuncs *cf)
{
	for (; cf->func != NULL; cf++)
	{
		DispatchTable[cf->cmd].func = cf->func;
	}
}


static EP_STAT
cmd_not_implemented(gdp_req_t *req,
		gdp_chan_t *chan)
{
	const char *peer = "(unknown)";
	size_t dl;

	// just ignore unknown commands
	ep_dbg_cprintf(Dbg, 1, "gdp_read_cb: Unknown packet cmd %d from %s\n",
			req->pkt.cmd, peer);

	// consume and discard remaining data
	dl = gdp_buf_drain(bufferevent_get_input(chan), req->pkt.dlen);
	if (dl < req->pkt.dlen)
	{
		// "can't happen" since that data is already in memory
		gdp_log(GDP_STAT_BUFFER_FAILURE,
			"gdp_read_cb: gdp_buf_drain failed:"
			" dlen = %u, drained = %zu\n",
			req->pkt.dlen, dl);
		// we are now probably out of sync; can we continue?
	}
	return GDP_STAT_NOT_IMPLEMENTED;
}


EP_STAT
_gdp_req_dispatch(gdp_req_t *req, gdp_chan_t *chan)
{
	EP_STAT estat;
	dispatch_ent_t *d;

	d = &DispatchTable[req->pkt.cmd];
	if (d->func == NULL)
		estat = cmd_not_implemented(req, chan);
	else
		estat = (*d->func)(req, chan);
	return estat;
}


/*
**	GDP_READ_CB --- data is available for reading from gdpd socket
*/

static void
gdp_read_cb(gdp_chan_t *chan, void *ctx)
{
	EP_STAT estat;
	gdp_pkt_hdr_t pkt;
	gdp_buf_t *ievb = bufferevent_get_input(chan);
	gcl_handle_t *gclh;
	gdp_req_t *req;

	ep_dbg_cprintf(Dbg, 50, "gdp_read_cb: fd %d\n", bufferevent_getfd(chan));

	estat = _gdp_pkt_in(&pkt, ievb);
	if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
		return;

	// find the handle for the associated GCL
	if (EP_UT_BITSET(GDP_PKT_HAS_ID, pkt.flags))
	{
		gclh = _gdp_gcl_cache_get(pkt.gcl_name, 0);
		if (gclh == NULL)
		{
			gcl_pname_t pbuf;

			gdp_gcl_printable_name(pkt.gcl_name, pbuf);
			ep_dbg_cprintf(Dbg, 1, "gdp_read_cb: GCL %s has no handle\n", pbuf);
		}
	}
	else
	{
		gclh = NULL;
	}

	// find the request
	if (gclh == NULL)
		req = NULL;
	else
		req = _gdp_req_find(gclh, pkt.rid);

	if (req == NULL)
	{
		estat = _gdp_req_new(pkt.cmd, gclh, 0, &req);
		if (!EP_STAT_ISOK(estat))
		{
			gdp_log(estat, "gdp_read_cb: cannot allocate request");
			return;
		}
		
		//XXX link request into GCL list??
	}

	// copy the temporary packet into the request
	pkt.dbuf = req->pkt.dbuf;
	memcpy(&req->pkt, &pkt, sizeof req->pkt);

	// invoke the command-specific (or ack-specific) function
	_gdp_req_dispatch(req, chan);

	// ASSERT all data from chan has been consumed

//	XXX KEEP_READING is not an option, since the header has been consumed
//	if (gclh == NULL || EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
//	{
//		ep_dbg_cprintf(Dbg, 44, "gdp_read_cb: keep reading\n");
//		return;
//	}

//	// if this a response and the RID isn't persistent, we're done with it
//	if (rif != NULL && pkt.cmd >= GDP_ACK_MIN &&
//			!EP_UT_BITSET(RINFO_PERSISTENT | RINFO_KEEP_MAPPING, rif->flags))
//		drop_rid_mapping(pkt.rid, conn);

	// return our status via the request
	ep_thr_mutex_lock(&req->mutex);
	req->stat = estat;
	req->flags |= GDP_REQ_DONE;
	if (ep_dbg_test(Dbg, 44))
	{
		gcl_pname_t pbuf;
		char ebuf[200];

		if (req->gclh != NULL)
			gdp_gcl_printable_name(req->gclh->gcl_name, pbuf);
		else
			(void) strlcpy(pbuf, "(none)", sizeof pbuf);
		ep_dbg_printf("gdp_read_cb: returning stat %s\n\tGCL %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf), pbuf);
	}
	ep_thr_cond_signal(&req->cond);
	ep_thr_mutex_unlock(&req->mutex);
}


/*
**	GDP_EVENT_CB --- events or errors occur on gdpd socket
*/

static EP_PRFLAGS_DESC	EventWhatFlags[] =
{
	{ BEV_EVENT_READING,	BEV_EVENT_READING,		"BEV_EVENT_READING"		},
	{ BEV_EVENT_WRITING,	BEV_EVENT_WRITING,		"BEV_EVENT_WRITING"		},
	{ BEV_EVENT_EOF,		BEV_EVENT_EOF,			"BEV_EVENT_EOF"			},
	{ BEV_EVENT_ERROR,		BEV_EVENT_ERROR,		"BEV_EVENT_ERROR"		},
	{ BEV_EVENT_TIMEOUT,	BEV_EVENT_TIMEOUT,		"BEV_EVENT_TIMEOUT"		},
	{ BEV_EVENT_CONNECTED,	BEV_EVENT_CONNECTED,	"BEV_EVENT_CONNECTED"	},
	{ 0, 0, NULL }
};

static void
gdp_event_cb(gdp_chan_t *chan, short events, void *ctx)
{
	bool exitloop = false;

	if (ep_dbg_test(Dbg, 25))
	{
		ep_dbg_printf("gdp_event_cb: fd %d: ", bufferevent_getfd(chan));
		ep_prflags(events, EventWhatFlags, ep_dbg_getfile());
		ep_dbg_printf("\n");
	}
	if (EP_UT_BITSET(BEV_EVENT_EOF, events))
	{
		gdp_buf_t *ievb = bufferevent_get_input(chan);
		size_t l = gdp_buf_getlength(ievb);

		ep_dbg_cprintf(Dbg, 1, "gdpd_event_cb: got EOF, %zu bytes left\n", l);
		exitloop = true;
	}
	if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
	{
		ep_dbg_cprintf(Dbg, 1, "gdpd_event_cb: error: %s\n",
				evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
		exitloop = true;
	}
	if (exitloop)
		event_base_loopexit(GdpIoEventBase, NULL);
}


/*
**	INITIALIZATION ROUTINES
*/

static EP_STAT
init_error(const char *msg, const char *where)
{
	int eno = errno;
	EP_STAT estat = ep_stat_from_errno(eno);

	gdp_log(estat, "gdp_init: %s: %s", where, msg);
	ep_app_error("gdp_init: %s: %s: %s", where, msg, strerror(eno));
	return estat;
}


/*
**	Base loop to be called for event-driven systems.
**	Their events should have already been added to the event base.
**
**		GdpIoEventLoopThread is also used by gdpd, hence non-static.
*/

pthread_t		_GdpIoEventLoopThread;

static void
event_loop_timeout(int fd, short what, void *ctx)
{
	struct event_loop_info *eli = ctx;

	ep_dbg_cprintf(Dbg, 49, "%s event loop timeout\n", eli->where);
}

static void *
run_event_loop(void *ctx)
{
	struct event_loop_info *eli = ctx;
	struct event_base *evb = eli->evb;
	long evdelay = ep_adm_getlongparam("gdp.rest.event.loopdelay", 100000L);
	
	// keep the loop alive if EVLOOP_NO_EXIT_ON_EMPTY isn't available
	long ev_timeout = ep_adm_getlongparam("gdp.rest.event.looptimeout", 30L);
	struct timeval tv = { ev_timeout, 0 };
	struct event *evtimer = event_new(evb, -1, EV_PERSIST,
			&event_loop_timeout, eli);
	event_add(evtimer, &tv);

	for (;;)
	{
		if (ep_dbg_test(Dbg, 20))
		{
			ep_dbg_printf("gdp_event_loop: starting up %s base loop\n",
					eli->where);
			event_base_dump_events(evb, ep_dbg_getfile());
		}

#ifdef EVLOOP_NO_EXIT_ON_EMPTY
		event_base_loop(evb, EVLOOP_NO_EXIT_ON_EMPTY);
#else
		event_base_loop(evb, 0);
#endif

		// shouldn't happen (?)
		if (ep_dbg_test(Dbg, 1))
		{
			ep_dbg_printf("gdp_event_loop: %s event_base_loop returned\n",
					eli->where);
			if (event_base_got_break(evb))
				ep_dbg_printf(" ... as a result of loopbreak\n");
			if (event_base_got_exit(evb))
				ep_dbg_printf(" ... as a result of loopexit\n");
		}
		if (evdelay > 0)
			ep_time_nanosleep(evdelay * 1000LL);		// avoid CPU hogging
	}
}

EP_STAT
_gdp_start_event_loop_thread(pthread_t *thr,
		struct event_base *evb,
		const char *where)
{
	struct event_loop_info *eli = ep_mem_malloc(sizeof *eli);

	eli->evb = evb;
	eli->where = where;
	if (pthread_create(thr, NULL, run_event_loop, eli) != 0)
		return init_error("cannot create event loop thread", where);
	else
		return EP_STAT_OK;
}


/*
**	_GDP_DO_INIT --- initialize this library
*/

static EP_DBG	EvlibDbg = EP_DBG_INIT("gdp.evlib", "GDP Eventlib");

void
evlib_log_cb(int severity, const char *msg)
{
	char *sev;
	char *sevstrings[] = { "debug", "msg", "warn", "error" };

	if (severity < 0 || severity > 3)
		sev = "?";
	else
		sev = sevstrings[severity];
	ep_dbg_cprintf(EvlibDbg, (4 - severity) * 10, "[%s] %s\n", sev, msg);
}

/*
**  Initialization, Part 1:
**		Initialize the various external libraries.
**		Set up the I/O event loop base.
**		Initialize the GCL cache.
*/

EP_STAT
_gdp_do_init_1(void)
{
	EP_STAT estat = EP_STAT_OK;

	ep_dbg_cprintf(Dbg, 4, "gdp_init: initializing\n");

	// initialize the EP library
	ep_lib_init(EP_LIB_USEPTHREADS);

	// register status strings
	_gdp_stat_init();

	// tell the event library that we're using pthreads
	if (evthread_use_pthreads() < 0)
		return init_error("cannot use pthreads", "gdp_init");

	// set up the event base
	if (GdpIoEventBase == NULL)
	{
		if (ep_dbg_test(EvlibDbg, 40))
		{
			// according to the book...
			//event_enable_debug_logging(EVENT_DBG_ALL);
			// according to the code...
			event_enable_debug_mode();
		}

		// Initialize for I/O events
		{
			struct event_config *ev_cfg = event_config_new();

			event_config_require_features(ev_cfg, 0);
			GdpIoEventBase = event_base_new_with_config(ev_cfg);
			if (GdpIoEventBase == NULL)
				estat = init_error("could not create event base", "gdp_init");
			event_config_free(ev_cfg);
			EP_STAT_CHECK(estat, goto fail0);

			event_set_log_callback(evlib_log_cb);
		}
	}

	estat = _gdp_gcl_cache_init();

fail0:
	return estat;
}


/*
**	Initialization, Part 2:
**		Open the connection to gdpd.
**		Start the event loop to service that connection.
**
**	This code is not called by gdpd.
*/

EP_STAT
_gdp_do_init_2(void)
{
	gdp_chan_t *chan;
	EP_STAT estat = EP_STAT_OK;

	// set up the bufferevent
	chan = bufferevent_socket_new(GdpIoEventBase,
						-1,
						BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE |
						BEV_OPT_DEFER_CALLBACKS);
	if (chan == NULL)
	{
		estat = init_error("could not allocate bufferevent", "gdp_init");
		goto fail0;
	}
	else
	{
		// attach it to a socket
		struct sockaddr_in sa;
		int gdpd_port = ep_adm_getintparam("swarm.gdp.controlport",
										GDP_PORT_DEFAULT);
		const char *peername = ep_adm_getstrparam("swarm.gdp.controlhost",
										"127.0.0.1");

		bufferevent_setcb(chan, gdp_read_cb, NULL, gdp_event_cb, NULL);
		bufferevent_enable(chan, EV_READ | EV_WRITE);

		// TODO at the moment just attaches to localhost; should
		//		do discovery or at least have a run time parameter
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = inet_addr(peername);
		sa.sin_port = htons(gdpd_port);
		if (bufferevent_socket_connect(chan, (struct sockaddr *) &sa,
									sizeof sa) < 0)
		{
			estat = init_error("could not connect to IPv4 socket", "gdp_init");
			goto fail1;
		}
		GdpChannel = chan;
		ep_dbg_cprintf(Dbg, 10, "gdp_init: listening on port %d, fd %d\n",
				gdpd_port, bufferevent_getfd(chan));
	}

	// create a thread to run the event loop
	estat = _gdp_start_event_loop_thread(&_GdpIoEventLoopThread,
										GdpIoEventBase, "I/O");
	EP_STAT_CHECK(estat, goto fail1);

	ep_dbg_cprintf(Dbg, 4, "gdp_init: success\n");
	return estat;

fail1:
	bufferevent_free(chan);

fail0:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 4, "gdp_init: failure: %s\n",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}
