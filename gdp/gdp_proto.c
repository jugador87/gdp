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


typedef struct gdp_conn_ctx
{
	const char			*peername;
} conn_t;

static struct bufferevent	*GdpPortBufferEvent;	// our primary protocol port


/***********************************************************************
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
static struct req_list	ReqFreeList = LIST_HEAD_INITIALIZER(ReqFreeList);
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
		req->dbuf = gdp_buf_new();
		gdp_buf_setlock(req->dbuf, &req->mutex);
	}

	req->cmd = cmd;
	req->gclh = gclh;
	req->recno = GDP_PKT_NO_RECNO;
	req->ts.stamp.tv_sec = TT_NOTIME;
	req->dlen = 0;
	req->stat = EP_STAT_OK;
	if (gclh == NULL || EP_UT_BITSET(GDP_REQ_SYNC, flags))
	{
		// just use constant zero; any value would be fine
		req->rid = GDP_PKT_NO_RID;
	}
	else
	{
		// allocate a new unique request id
		req->rid = _gdp_rid_new(gclh);
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
		ep_thr_mutex_lock(&req->gclh->mutex);
	LIST_REMOVE(req, list);
	if (req->gclh != NULL)
		ep_thr_mutex_unlock(&req->gclh->mutex);

	// add it to the free list
	ep_thr_mutex_lock(&ReqFreeListMutex);
	LIST_INSERT_HEAD(&ReqFreeList, req, list);
	ep_thr_mutex_unlock(&ReqFreeListMutex);

	ep_thr_mutex_unlock(&req->mutex);
}

void
_gdp_req_freeall(struct req_list *reqlist)
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
		if (req->rid != rid)
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
	gdp_pkt_hdr_t pkt;
	gcl_handle_t *gclh = req->gclh;

	ep_dbg_cprintf(Dbg, 45, "gdp_req_send: cmd=%d\n", req->cmd);
	EP_ASSERT(gclh != NULL);

	// initialize packet header
	_gdp_pkt_hdr_init(&pkt, req->cmd, (gdp_rid_t) gclh, gclh->gcl_name);
	pkt.rid = req->rid;
	if (msg != NULL)
	{
		pkt.msgno = msg->msgno;
		pkt.ts = msg->ts;
		pkt.dbuf = msg->dbuf;
	}

	// register this handle so we can process the results
	//		(it's likely that it's already in the cache)
	_gdp_gcl_cache_add(gclh, 0);

	// link the request to the GCL
	ep_thr_mutex_lock(&gclh->mutex);
	LIST_INSERT_HEAD(&gclh->reqs, req, list);
	ep_thr_mutex_unlock(&gclh->mutex);

	// write the message out
	estat = _gdp_pkt_out(&pkt, bufferevent_get_output(GdpPortBufferEvent));
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
	ep_dbg_cprintf(Dbg, 43, "gdp_invoke: cmd=%d\n", cmd);

	/*
	**  Top Half: sending the command
	*/

	// get an associated request
	estat = _gdp_req_new(cmd, gclh, GDP_REQ_SYNC, &req);
	EP_STAT_CHECK(estat, goto fail0);

	req->cmd = cmd;

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
	if (event_base_got_exit(GdpEventBase))
	{
		ep_dbg_cprintf(Dbg, 1, "gdp_invoke: exiting on loopexit\n");
		estat = GDP_STAT_INTERNAL_ERROR;
	}

	if (EP_UT_BITSET(GDP_REQ_DONE, req->flags))
	{
		estat = req->stat;
		if (msg != NULL)
		{
			msg->msgno = req->recno;
			msg->ts = req->ts;
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
**			pkt --- the incoming packet off the wire.
**			req --- ??? XXX
**			bev --- the libevent channel from which this pdu came
**			conn --- the connection context (used for anything? XXX)
**
**		They can return GDP_STAT_KEEP_READING to tell the upper
**		layer that the whole packet hasn't been read yet.
**
***********************************************************************/

static EP_STAT
ack_success(gdp_pkt_hdr_t *pkt,
		gdp_req_t *req,
		struct bufferevent *bev,
		struct gdp_conn_ctx *conn)
{
	EP_STAT estat = GDP_STAT_FROM_ACK(pkt->cmd);
	size_t tocopy = pkt->dlen;
	gcl_handle_t *gclh = req->gclh;

	// we require a request
	if (req == NULL)
	{
		estat = GDP_STAT_PROTOCOL_FAIL;
		gdp_log(estat, "ack_success: null request");
	}

	//	If we started with no gcl id, adopt from incoming packet.
	//	This can happen when creating a GCL.
	if (gclh != NULL)
	{
		if (gdp_gcl_name_is_zero(gclh->gcl_name))
			memcpy(gclh->gcl_name, pkt->gcl_name, sizeof gclh->gcl_name);

		req->ts = pkt->ts;

		if (req->dbuf != NULL)
		{
			while (tocopy > 0)
			{
				int i;

				i = evbuffer_remove_buffer(bufferevent_get_input(bev),
						req->dbuf, tocopy);
				if (i > 0)
				{
					tocopy -= i;
				}
				else
				{
					// XXX should give some error here
					break;
				}
			}
		}
		else
			gdp_buf_drain(bufferevent_get_input(bev), tocopy);
	}

	return estat;
}


static EP_STAT
nak(int cmd, const char *where)
{
	ep_dbg_cprintf(Dbg, 2, "nak_%s: received %d\n", where, cmd);
	return GDP_STAT_FROM_NAK(cmd);
}


static EP_STAT
nak_client(gdp_pkt_hdr_t *pkt,
		gdp_req_t *req,
		struct bufferevent *bev,
		struct gdp_conn_ctx *conn)
{
	return nak(pkt->cmd, "client");
}


static EP_STAT
nak_server(gdp_pkt_hdr_t *pkt,
		gdp_req_t *req,
		struct bufferevent *bev,
		struct gdp_conn_ctx *conn)
{
	return nak(pkt->cmd, "server");
}



/*
**	Command/Ack/Nak Dispatch Table
*/

typedef EP_STAT cmdfunc_t(gdp_pkt_hdr_t *pkt,
						gdp_req_t *req,
						struct bufferevent *bev,
						struct gdp_conn_ctx *conn);

typedef struct
{
	cmdfunc_t	*func;		// function to call
	const char	*name;		// name of command (for debugging)
	int			dummy;		// unused as yet
} dispatch_ent_t;

#define NOENT		{ NULL, NULL, 0 }

static dispatch_ent_t	DispatchTable[256] =
{
	{ NULL,		"CMD_KEEPALIVE",		0 },			// 0
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
	{ NULL,				"CMD_PING",				0 },			// 64
	{ NULL,				"CMD_HELLO",			0 },			// 65
	{ NULL,				"CMD_CREATE",			0 },			// 66
	{ NULL,				"CMD_OPEN_AO",			0 },			// 67
	{ NULL,				"CMD_OPEN_RO",			0 },			// 68
	{ NULL,				"CMD_CLOSE",			0 },			// 69
	{ NULL,				"CMD_READ",				0 },			// 70
	{ NULL,				"CMD_PUBLISH",			0 },			// 71
	{ NULL,				"CMD_SUBSCRIBE",		0 },			// 72
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
	{ ack_success,		"ACK_SUCCESS",			0 },			// 128
	{ ack_success,		"ACK_DATA_CREATED",		0 },			// 129
	{ ack_success,		"ACK_DATA_DEL",			0 },			// 130
	{ ack_success,		"ACK_DATA_VALID",		0 },			// 131
	{ ack_success,		"ACK_DATA_CHANGED",		0 },			// 132
	{ ack_success,		"ACK_DATA_CONTENT",		0 },			// 133
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
	{ nak_client,		"NAK_C_BADREQ",			0 },			// 192
	{ nak_client,		"NAK_C_UNAUTH",			0 },			// 193
	{ nak_client,		"NAK_C_BADOPT",			0 },			// 194
	{ nak_client,		"NAK_C_FORBIDDEN",		0 },			// 195
	{ nak_client,		"NAK_C_NOTFOUND",		0 },			// 196
	{ nak_client,		"NAK_C_METHNOTALLOWED", 0 },			// 197
	{ nak_client,		"NAK_C_NOTACCEPTABLE",	0 },			// 198
	NOENT,				// 199
	NOENT,				// 200
	NOENT,				// 201
	NOENT,				// 202
	NOENT,				// 203
	{ nak_client,		"NAK_C_PRECONFAILED",	0 },			// 204
	{ nak_client,		"NAK_C_TOOLARGE",		0 },			// 205
	NOENT,				// 206
	{ nak_client,		"NAK_C_UNSUPMEDIA",		0 },			// 207
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
	{ nak_server,		"NAK_S_INTERNAL",		0 },			// 224
	{ nak_server,		"NAK_S_NOTIMPL",		0 },			// 225
	{ nak_server,		"NAK_S_BADGATEWAY",		0 },			// 226
	{ nak_server,		"NAK_S_SVCUNAVAIL",		0 },			// 227
	{ nak_server,		"NAK_S_GWTIMEOUT",		0 },			// 228
	{ nak_server,		"NAK_S_PROXYNOTSUP",	0 },			// 229
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


EP_STAT
cmd_not_implemented(gdp_pkt_hdr_t *pkt,
		gdp_req_t *req,
		struct bufferevent *bev,
		struct gdp_conn_ctx *conn)
{
	const char *peer = conn->peername;
	size_t dl;

	if (peer == NULL)
		peer = "(unknown)";
	// just ignore unknown commands
	ep_dbg_cprintf(Dbg, 1, "gdp_read_cb: Unknown packet cmd %d from %s\n",
			pkt->cmd, peer);

	// consume and discard remaining data
	if ((dl = gdp_buf_drain(bufferevent_get_input(bev), pkt->dlen)) < pkt->dlen)
	{
		// "can't happen" since that data is already in memory
		gdp_log(GDP_STAT_BUFFER_FAILURE,
			"gdp_read_cb: gdp_buf_drain failed:"
			" dlen = %u, drained = %zu\n",
			pkt->dlen, dl);
		// we are now probably out of sync; can we continue?
	}
	return GDP_STAT_NOT_IMPLEMENTED;
}


/*
**	GDP_READ_CB --- data is available for reading from gdpd socket
*/

static void
gdp_read_cb(struct bufferevent *bev, void *ctx)
{
	EP_STAT estat;
	gdp_pkt_hdr_t pkt;
	gdp_buf_t *ievb = bufferevent_get_input(bev);
	struct gdp_conn_ctx *conn = ctx;
	gcl_handle_t *gclh;
	gdp_req_t *req;
	dispatch_ent_t *d;

	ep_dbg_cprintf(Dbg, 50, "gdp_read_cb: fd %d\n", bufferevent_getfd(bev));

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
	if (EP_UT_BITSET(GDP_PKT_NO_RID, pkt.flags))
	{
		req = NULL;
	}
	else if (gclh == NULL)
	{
		char rbuf[100];

		gdp_log(GDP_STAT_PROTOCOL_FAIL,
				"gdp_read_cb: rid %s with no GCL ID",
				gdp_rid_tostr(pkt.rid, rbuf, sizeof rbuf));
		req = NULL;
	}
	else if ((req = _gdp_req_find(gclh, pkt.rid)) == NULL)
	{
		gcl_pname_t gbuf;
		char rbuf[100];

		gdp_log(GDP_STAT_PROTOCOL_FAIL,
				"gdp_read_cb: cannot find rid %s, GCL %s",
				gdp_rid_tostr(pkt.rid, rbuf, sizeof rbuf),
				gdp_gcl_printable_name(gclh->gcl_name, gbuf));
	}

	// associate the request with the gcl
	if (req != NULL)
	{
		req->gclh = gclh;
	}
	else if (gclh != NULL)
	{
		gcl_pname_t gbuf;

		gdp_log(GDP_STAT_PROTOCOL_FAIL,
				"gdp_read_cb: no rid for gcl %s",
				gdp_gcl_printable_name(gclh->gcl_name, gbuf));
	}

	// invoke the command-specific (or ack-specific) function
	d = &DispatchTable[pkt.cmd];
	if (d->func == NULL)
		estat = cmd_not_implemented(&pkt, req, bev, conn);
	else
		estat = (*d->func)(&pkt, req, bev, conn);

	// ASSERT all data from bev has been consumed

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

	if (req != NULL)
	{
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
gdp_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	bool exitloop = false;

	if (ep_dbg_test(Dbg, 25))
	{
		ep_dbg_printf("gdp_event_cb: fd %d: ", bufferevent_getfd(bev));
		ep_prflags(events, EventWhatFlags, ep_dbg_getfile());
		ep_dbg_printf("\n");
	}
	if (EP_UT_BITSET(BEV_EVENT_EOF, events))
	{
		gdp_buf_t *ievb = bufferevent_get_input(bev);
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
		event_base_loopexit(GdpEventBase, NULL);
}


/*
**	INITIALIZATION ROUTINES
*/

EP_STAT
init_error(const char *msg)
{
	int eno = errno;
	EP_STAT estat = ep_stat_from_errno(eno);

	gdp_log(estat, "gdp_init: %s", msg);
	ep_app_error("gdp_init: %s: %s", msg, strerror(eno));
	return estat;
}


/*
**	Base loop to be called for event-driven systems.
**	Their events should have already been added to the event base.
**
**		GdpIoEventLoopThread is also used by gdpd, hence non-static.
*/

static pthread_t	AcceptEventLoopThread;
pthread_t			GdpIoEventLoopThread;

void *
gdp_run_accept_event_loop(void *ctx)
{
	struct event_base *evb = ctx;
	long evdelay = ep_adm_getlongparam("gdp.rest.event.loopdelay", 100000);

	if (evb == NULL)
		evb = GdpEventBase;

	for (;;)
	{
		if (ep_dbg_test(Dbg, 20))
		{
			ep_dbg_printf("gdp_event_loop: starting up base loop\n");
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
			ep_dbg_printf("gdp_event_loop: event_base_loop returned\n");
			if (event_base_got_break(evb))
				ep_dbg_printf(" ... as a result of loopbreak\n");
			if (event_base_got_exit(evb))
				ep_dbg_printf(" ... as a result of loopexit\n");
		}
		if (evdelay > 0)
			ep_time_nanosleep(evdelay * 1000LL);		// avoid CPU hogging
	}
}

void *
gdp_run_io_event_loop(void *ctx)
{
	struct event_base *evb = ctx;
	long evdelay = ep_adm_getlongparam("gdp.rest.event.loopdelay", 100000);

	if (evb == NULL)
	{
		gdp_log(EP_STAT_ERROR, "gdp_run_io_event_loop: null event_base");
		return NULL;
	}
	while (true)
	{
		// TODO: what if user compiling doesn't have libevent 2.1-alpha?
#ifdef EVLOOP_NO_EXIT_ON_EMPTY
		event_base_loop(evb, EVLOOP_NO_EXIT_ON_EMPTY);
#else
		event_base_loop(evb, 0);
#endif
		// shouldn't happen (?)
		if (ep_dbg_test(Dbg, 1))
		{
			ep_dbg_printf("gdp_event_loop: event_base_loop returned\n");
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
_gdp_start_accept_event_loop_thread(struct event_base *evb)
{
	if (pthread_create(&AcceptEventLoopThread, NULL,
				gdp_run_accept_event_loop, evb) != 0)
		return init_error("cannot create event loop thread");
	else
		return EP_STAT_OK;
}

EP_STAT
_gdp_start_io_event_loop_thread(struct event_base *evb)
{
	if (pthread_create(&GdpIoEventLoopThread, NULL,
						gdp_run_io_event_loop, evb) != 0)
	{
		return init_error("cannot create io event loop thread");
	}
	else
	{
		return EP_STAT_OK;
	}
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

EP_STAT
_gdp_do_init(bool run_event_loop)
{
	static bool inited = false;
	EP_STAT estat = EP_STAT_OK;
	struct bufferevent *bev;
	extern void gdp_stat_init(void);

	if (inited)
		return EP_STAT_OK;
	inited = true;

	ep_dbg_cprintf(Dbg, 4, "gdp_init: initializing\n");

	// initialize the EP library
	ep_lib_init(EP_LIB_USEPTHREADS);

	// register status strings
	gdp_stat_init();

	// tell it we're using pthreads
	if (evthread_use_pthreads() < 0)
		return init_error("cannot use pthreads");

	// set up the event base
	if (GdpEventBase == NULL)
	{
		if (ep_dbg_test(EvlibDbg, 40))
		{
			// according to the book...
			//event_enable_debug_logging(EVENT_DBG_ALL);
			// according to the code...
			event_enable_debug_mode();
		}

		// Initialize the EVENT library
		{
			struct event_config *ev_cfg = event_config_new();

			event_config_require_features(ev_cfg, 0);
			GdpEventBase = event_base_new_with_config(ev_cfg);
			if (GdpEventBase == NULL)
				estat = init_error("could not create event base");
			event_config_free(ev_cfg);
			EP_STAT_CHECK(estat, goto fail0);

			event_set_log_callback(evlib_log_cb);
		}
	}

	estat = _gdp_gcl_cache_init();
	EP_STAT_CHECK(estat, goto fail0);

	// set up the bufferevent
	bev = bufferevent_socket_new(GdpEventBase,
						-1,
						BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
	if (bev == NULL)
	{
		estat = init_error("could not allocate bufferevent");
		goto fail0;
	}
	else
	{
		// attach it to a socket
		conn_t *conn = ep_mem_zalloc(sizeof *conn);
		struct sockaddr_in sa;
		int gdpd_port = ep_adm_getintparam("swarm.gdp.controlport",
										GDP_PORT_DEFAULT);

		conn->peername = ep_adm_getstrparam("swarm.gdp.controlhost",
										"127.0.0.1");
		bufferevent_setcb(bev, gdp_read_cb, NULL, gdp_event_cb, conn);
		bufferevent_enable(bev, EV_READ | EV_WRITE);

		// TODO at the moment just attaches to localhost; should
		//		do discovery or at least have a run time parameter
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = inet_addr(conn->peername);
		sa.sin_port = htons(gdpd_port);
		if (bufferevent_socket_connect(bev, (struct sockaddr *) &sa,
									sizeof sa) < 0)
		{
			estat = init_error("could not connect to IPv4 socket");
			goto fail1;
		}
		GdpPortBufferEvent = bev;
		ep_dbg_cprintf(Dbg, 10, "gdp_init: listening on port %d, fd %d\n",
				gdpd_port, bufferevent_getfd(bev));
	}

	// create a thread to run the event loop
	if (run_event_loop)
		estat = _gdp_start_accept_event_loop_thread(GdpEventBase);
	EP_STAT_CHECK(estat, goto fail1);

	ep_dbg_cprintf(Dbg, 4, "gdp_init: success\n");
	return estat;

fail1:
	bufferevent_free(bev);

fail0:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 4, "gdp_init: failure: %s\n",
					ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}
