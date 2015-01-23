/* vim: set ai sw=4 sts=4 ts=4 :*/

#include "gdp.h"
#include "gdp_priv.h"

#include <ep/ep_dbg.h>
#include <ep/ep_log.h>

#include <string.h>

/*
**	This implements the GDP Protocol.
**
**	In the future this may need to be extended to have knowledge of
**	TSN/AVB, but for now we don't worry about that.
*/


static EP_DBG	Dbg = EP_DBG_INIT("gdp.proto", "GDP protocol processing");

static uint8_t	RoutingLayerAddr[32] =
	{
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
		0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
	};



/*
**	GDP_INVOKE --- do a remote invocation to the GDP daemon
**
**		This acts as an remote procedure call.  In particular it
**		waits to get a result, which makes it inappropriate for
**		instances where multiple commands are in flight, or
**		where a command can return multiple values (e.g., subscribe).
*/

EP_STAT
_gdp_invoke(gdp_req_t *req)
{
	EP_STAT estat;
	EP_TIME_SPEC timeout;
	static long delta_timeout = -1;
	int retries = ep_adm_getintparam("swarm.gdp.invoke.retries", 3);

	EP_ASSERT_POINTER_VALID(req);
	if (ep_dbg_test(Dbg, 22))
	{
		ep_dbg_printf("\n*** _gdp_invoke(%p): %s (%d), gcl@%p:\n\t",
				req,
				_gdp_proto_cmd_name(req->pdu->cmd),
				req->pdu->cmd,
				req->gcl);
		gdp_datum_print(req->pdu->datum, ep_dbg_getfile());
	}

	if (delta_timeout < 0)
	{
		delta_timeout = ep_adm_getlongparam("swarm.gdp.invoke.timeout",
								10000L);
	}

	// loop to allow for retransmissions
	do
	{
		/*
		**  Top Half: sending the command
		*/

		estat = _gdp_req_send(req);
		EP_STAT_CHECK(estat, goto fail0);

		/*
		**  Bottom Half: read the response
		*/

		// wait until we receive a result
		ep_dbg_cprintf(Dbg, 37, "gdp_invoke: waiting on %p\n", &req->cond);
		ep_time_deltanow(delta_timeout * INT64_C(1000000), &timeout);
		ep_thr_mutex_lock(&req->mutex);
		estat = EP_STAT_OK;
		while (!EP_UT_BITSET(GDP_REQ_DONE, req->flags))
		{
			int e = ep_thr_cond_wait(&req->cond, &req->mutex, &timeout);
			ep_dbg_cprintf(Dbg, 52, "  ... got %d\n", e);
			if (e != 0)
			{
				estat = ep_stat_from_errno(e);
				break;
			}
		}
		if (EP_STAT_ISOK(estat))
			estat = req->stat;
		ep_thr_mutex_unlock(&req->mutex);

#if 0
		//XXX what status will/should we return?
		if (event_base_got_exit(GdpIoEventBase))
		{
			ep_dbg_cprintf(Dbg, 1, "gdp_invoke: exiting on loopexit\n");
			estat = GDP_STAT_INTERNAL_ERROR;
		}
#endif
		// ok, done!
	} while (!EP_STAT_ISOK(estat) && retries-- > 0);

fail0:
	if (ep_dbg_test(Dbg, 22))
	{
		char ebuf[200];

		ep_dbg_printf("gdp_invoke: returning %s\n  ",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		_gdp_req_dump(req, ep_dbg_getfile());
	}
	return estat;
}



/***********************************************************************
**
**	Protocol processing (CMD/ACK/NAK)
**
**		All of these take as parameters:
**			req --- the request information (including packet header)
**
**		They can return GDP_STAT_KEEP_READING to tell the upper
**		layer that the whole packet hasn't been read yet.
**
***********************************************************************/


/*
**  ACKs (success)
*/

static EP_STAT
ack_success(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_gcl_t *gcl;

	// we require a request
	estat = GDP_STAT_PROTOCOL_FAIL;
	if (req == NULL)
		ep_log(estat, "ack_success: null request");
	else if (req->pdu->datum == NULL)
		ep_log(estat, "ack_success: null datum");
	else
		estat = GDP_STAT_FROM_ACK(req->pdu->cmd);
	EP_STAT_CHECK(estat, goto fail0);

	gcl = req->gcl;

	//	If we started with no gcl id, adopt from incoming packet.
	//	This can happen when creating a GCL.
	if (gcl != NULL && !gdp_name_is_valid(gcl->name))
	{
		memcpy(gcl->name, req->pdu->src, sizeof gcl->name);
		gdp_printable_name(gcl->name, gcl->pname);
	}

fail0:
	return estat;
}


/*
**  NAK_CLIENT, NAK_SERVER --- handle NAKs (negative acknowlegements)
*/

static EP_STAT
nak_client(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 2, "nak_client: received %d\n", req->pdu->cmd);
	return GDP_STAT_FROM_C_NAK(req->pdu->cmd);
}


static EP_STAT
nak_server(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 2, "nak_server: received %d\n", req->pdu->cmd);
	return GDP_STAT_FROM_S_NAK(req->pdu->cmd);
}


static EP_STAT
nak_router(gdp_req_t *req)
{
	ep_dbg_cprintf(Dbg, 2, "nak_router: received %d\n", req->pdu->cmd);
	return GDP_STAT_FROM_S_NAK(req->pdu->cmd);
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
	{ NULL,				"CMD_KEEPALIVE"			},			// 0
	{ NULL,				"CMD_ADVERTISE"			},			// 1
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
	{ NULL,				"CMD_MULTIREAD",		},			// 73
	{ NULL,				"CMD_GETMETADATA",		},			// 74
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
	{ nak_client,		"NAK_C_CONFLICT"		},			// 201
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
	{ nak_router,		"NAK_R_NOROUTE"			},			// 240
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


/*
**  Add any additional command functions.
**		Applications that add additional functionality (e.g.,
**		gdplogd) can add implementations by calling this function.
*/

void
_gdp_register_cmdfuncs(struct cmdfuncs *cf)
{
	for (; cf->func != NULL; cf++)
	{
		DispatchTable[cf->cmd].func = cf->func;
	}
}


/*
**  Called for unimplemented commands
*/

static EP_STAT
cmd_not_implemented(gdp_req_t *req)
{
	// just ignore unknown commands
	if (ep_dbg_test(Dbg, 1))
	{
		ep_dbg_printf("gdp_req_dispatch: Unknown cmd, req:\n");
		_gdp_req_dump(req, ep_dbg_getfile());
	}

	return GDP_STAT_NOT_IMPLEMENTED;
}


/*
**  Dispatch command to implementation function.
*/

EP_STAT
_gdp_req_dispatch(gdp_req_t *req)
{
	EP_STAT estat;
	dispatch_ent_t *d;

	d = &DispatchTable[req->pdu->cmd];
	if (d->func == NULL)
		estat = cmd_not_implemented(req);
	else
		estat = (*d->func)(req);
	return estat;
}


/*
**  Advertise our existence (and possibly more!)
*/

EP_STAT
_gdp_advertise(EP_STAT (*func)(gdp_buf_t *, void *), void *ctx)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;
	gdp_chan_t *chan = _GdpChannel;
	uint32_t reqflags = 0;

	ep_dbg_cprintf(Dbg, 39, "_gdp_advertise:\n");

	// create a new request and point it at the routing layer
	estat = _gdp_req_new(GDP_CMD_ADVERTISE, NULL, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	memcpy(req->pdu->dst, RoutingLayerAddr, sizeof req->pdu->dst);

	// add any additional information to advertisement
	if (func != NULL)
		estat = func(req->pdu->datum->dbuf, ctx);

	// send the request
	estat = _gdp_req_send(req);

	// there is no reply
	_gdp_req_free(req);

fail0:
	if (ep_dbg_test(Dbg, 10))
	{
		char ebuf[100];

		ep_dbg_printf("_gdp_advertise => %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	return estat;
}


/*
**  Advertise me only
*/

EP_STAT
_gdp_advertise_me(void)
{
	return _gdp_advertise(NULL, NULL);
}
