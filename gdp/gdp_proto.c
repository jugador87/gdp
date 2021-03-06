/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	This implements the GDP Protocol.
**
**	In the future this may need to be extended to have knowledge of
**	TSN/AVB, but for now we don't worry about that.
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
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

#include "gdp.h"
#include "gdp_priv.h"

#include <ep/ep_dbg.h>
#include <ep/ep_log.h>

#include <string.h>
#include <sys/errno.h>


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

#define MILLISECONDS		* INT64_C(1000000)

EP_STAT
_gdp_invoke(gdp_req_t *req)
{
	EP_STAT estat = EP_STAT_OK;
	EP_TIME_SPEC abs_to;
	long delta_to;				// how long to wait for a response
	int retries;				// how many times to retry
	long retry_delay;			// how long to delay between retries
	EP_TIME_SPEC delta_ts;
	const char *cmdname;

	EP_ASSERT_POINTER_VALID(req);
	cmdname = _gdp_proto_cmd_name(req->pdu->cmd);
	if (ep_dbg_test(Dbg, 11))
	{
		ep_dbg_printf("\n*** _gdp_invoke(%p): %s (%d), gcl@%p:\n\t",
				req,
				cmdname,
				req->pdu->cmd,
				req->gcl);
		_gdp_datum_dump(req->pdu->datum, ep_dbg_getfile());
	}
	EP_ASSERT(req->state == GDP_REQ_ACTIVE);

	// scale timeout to milliseconds
	delta_to = ep_adm_getlongparam("swarm.gdp.invoke.timeout", 10000L);
	ep_time_from_nsec(delta_to * INT64_C(1000000), &delta_ts);
	retry_delay = ep_adm_getlongparam("swarm.gdp.invoke.retrydelay", 5000L);

	// loop to allow for retransmissions
	retries = ep_adm_getintparam("swarm.gdp.invoke.retries", 3);
	if (retries < 1)
		retries = 1;
	while (retries-- > 0)
	{
		/*
		**  Top Half: sending the command
		*/

		ep_dbg_cprintf(Dbg, 36,
				"_gdp_invoke: sending %d, retries=%d\n",
				req->pdu->cmd, retries);

		estat = _gdp_req_send(req);
		EP_STAT_CHECK(estat, continue);

		/*
		**  Bottom Half: read the response
		*/

		// wait until we receive a result
		ep_dbg_cprintf(Dbg, 37, "_gdp_invoke: waiting on %p\n", req);
		ep_time_deltanow(&delta_ts, &abs_to);
		estat = EP_STAT_OK;
		req->state = GDP_REQ_WAITING;
		while (!EP_UT_BITSET(GDP_REQ_DONE, req->flags))
		{
			// cond_wait will unlock the mutex
			int e = ep_thr_cond_wait(&req->cond, &req->mutex, &abs_to);

			char ebuf[100];
			ep_dbg_cprintf(Dbg, 52,
					"_gdp_invoke wait: got %d, done=%d, state=%d, stat=%s\n",
					e, EP_UT_BITSET(GDP_REQ_DONE, req->flags), req->state,
					ep_stat_tostr(req->stat, ebuf, sizeof ebuf));
			if (e != 0)
			{
				estat = ep_stat_from_errno(e);
				if (e != ETIMEDOUT)			// retry on timeouts
					retries = 0;
				break;
			}
		}
		req->state = GDP_REQ_ACTIVE;
		if (EP_STAT_ISOK(estat))
		{
			estat = req->stat;

			// if we succeeded or it's our fault, don't try again
			if (EP_STAT_ISOK(estat) || GDP_STAT_IS_C_NAK(estat))
			{
				break;				// we're done, don't retry
			}
			else if (EP_STAT_IS_SAME(estat, GDP_STAT_NAK_NOROUTE) &&
					EP_UT_BITSET(GDP_REQ_ROUTEFAIL, req->flags))
			{
				// route failure on open: don't retry
				break;
			}
			else if (retries > 0)
			{
				_gdp_req_unsend(req);
				ep_time_nanosleep(retry_delay MILLISECONDS);
			}
		}
		else
		{
			// if the cond_wait failed, we have to pull the req off the gcl list
			_gdp_req_unsend(req);

			// if ETIMEDOUT, maybe the router had a glitch:
			//   wait and try again
			if (retries > 0)
				ep_time_nanosleep(retry_delay MILLISECONDS);
		}
	}

	if (ep_dbg_test(Dbg, 22))
	{
		char ebuf[200];

		flockfile(ep_dbg_getfile());
		ep_dbg_printf("gdp_invoke(%s) <<< %s\n  ",
				cmdname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		ep_dbg_printf("\n");
		funlockfile(ep_dbg_getfile());
	}
	return estat;
}



/***********************************************************************
**
**	Protocol processing (CMD/ACK/NAK)
**
**		All of these take as parameters:
**			req --- the request information (including PDU header)
**
**		They can return GDP_STAT_KEEP_READING to tell the upper
**		layer that the whole PDU hasn't been read yet.
**
***********************************************************************/


/*
**  Common code for ACKs and NAKs
*/

static EP_STAT
acknak(gdp_req_t *req, const char *where, bool reuse_pdu)
{
	// we require a request
	if (req == NULL)
	{
		ep_log(GDP_STAT_PROTOCOL_FAIL, "%s: null request", where);
		return GDP_STAT_PROTOCOL_FAIL;
	}

	ep_dbg_cprintf(Dbg, 8, "%s: received %d\n", where, req->pdu->cmd);

	// we want to re-use caller's datum for (e.g.) read commands
	if (req->rpdu != NULL && req->rpdu != req->pdu && reuse_pdu)
	{
		if (req->pdu->datum != NULL)
		{
			if (ep_dbg_test(Dbg, 43))
			{
				ep_dbg_printf("%s: reusing old datum for req %p\n   ",
						where, req);
				_gdp_datum_dump(req->pdu->datum, ep_dbg_getfile());
			}

			// don't need the old dbuf
			gdp_buf_free(req->pdu->datum->dbuf);

			// copy the contents of the new message over the old
			memcpy(req->pdu->datum, req->rpdu->datum, sizeof *req->pdu->datum);

			// we no longer need the new message
			req->rpdu->datum->dbuf = NULL;
			gdp_datum_free(req->rpdu->datum);

			// point the new PDU at the old datum
			req->rpdu->datum = req->pdu->datum;
			EP_ASSERT(req->rpdu->datum->inuse);
		}

		// can now drop the old pdu and switch to the new one
		req->pdu->datum = NULL;
		_gdp_pdu_free(req->pdu);
		req->pdu = req->rpdu;
		req->rpdu = NULL;
	}
	return EP_STAT_OK;
}


/*
**  ACKs (success)
*/

static EP_STAT
ack(gdp_req_t *req, const char *where)
{
	EP_STAT estat;

	estat = acknak(req, where, true);
	EP_STAT_CHECK(estat, return estat);

	if (req->pdu->datum == NULL)
	{
		ep_log(estat, "ack: null datum");
		estat = EP_STAT_OK;
	}
	else
	{
		estat = GDP_STAT_FROM_ACK(req->pdu->cmd);
	}
	return estat;
}


static EP_STAT
ack_success(gdp_req_t *req)
{
	EP_STAT estat;
	gdp_gcl_t *gcl;

	estat = ack(req, "ack_success");
	EP_STAT_CHECK(estat, goto fail0);

	// mark this request as active (for subscriptions)
	ep_time_now(&req->act_ts);

	//	If we started with no gcl id, adopt from incoming PDU.
	//	This can happen when creating a GCL.
	gcl = req->gcl;
	if (gcl != NULL && !gdp_name_is_valid(gcl->name))
	{
		memcpy(gcl->name, req->pdu->src, sizeof gcl->name);
		gdp_printable_name(gcl->name, gcl->pname);
	}

fail0:
	return estat;
}

static EP_STAT
ack_data_content(gdp_req_t *req)
{
	EP_STAT estat;

	estat = ack_success(req);
	EP_STAT_CHECK(estat, return estat);

	// keep track of the highest record number we've read
	if (req->gcl->nrecs < req->pdu->datum->recno)
		req->gcl->nrecs = req->pdu->datum->recno;

	// keep track of how many more records we expect
	if (req->numrecs > 0)
		req->numrecs--;

	// do read filtering if requested
	if (req->gcl->readfilter != NULL)
		estat = req->gcl->readfilter(req->pdu->datum, req->gcl->readfpriv);

	return estat;
}


/*
**  NAKs (failures)
*/

static EP_STAT
nak(gdp_req_t *req, const char *where)
{
	EP_STAT estat;

	estat = acknak(req, where, true);
	return estat;
}


static EP_STAT
nak_client(gdp_req_t *req)
{
	nak(req, "nak_client");
	return GDP_STAT_FROM_C_NAK(req->pdu->cmd);
}


static EP_STAT
nak_server(gdp_req_t *req)
{
	nak(req, "nak_server");
	return GDP_STAT_FROM_S_NAK(req->pdu->cmd);
}


static EP_STAT
nak_router(gdp_req_t *req)
{
	acknak(req, "nak_router", false);
	return GDP_STAT_FROM_R_NAK(req->rpdu->cmd);
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
	{ NULL,				"CMD_WITHDRAW"			},			// 2
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
	{ NULL,				"CMD_APPEND"			},			// 71
	{ NULL,				"CMD_SUBSCRIBE"			},			// 72
	{ NULL,				"CMD_MULTIREAD"			},			// 73
	{ NULL,				"CMD_GETMETADATA"		},			// 74
	{ NULL,				"CMD_OPEN_RA"			},			// 75
	{ NULL,				"CMD_NEWEXTENT"			},			// 76
	{ NULL,				"CMD_FWD_APPEND"		},			// 77
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
	{ ack_data_content,	"ACK_DATA_CONTENT"		},			// 133
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
	{ nak_client,		"NAK_C_GONE"			},			// 202
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
	{ nak_server,		"NAK_S_LOSTSUB"			},			// 239
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

	if (d->name != NULL)
	{
		return d->name;
	}
	else
	{
		// not thread safe, but shouldn't happen
		static char buf[10];

		snprintf(buf, sizeof buf, "%d", cmd);
		return buf;
	}
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
		ep_dbg_printf("_gdp_req_dispatch: Unknown cmd, req:\n");
		_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
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
	int cmd;

	// if this is a response, rpdu will be set
	if (req->rpdu == NULL)
		cmd = req->pdu->cmd;
	else
		cmd = req->rpdu->cmd;

	if (ep_dbg_test(Dbg, 18))
	{
		ep_dbg_printf("_gdp_req_dispatch >>> %s (%d)",
				_gdp_proto_cmd_name(cmd), cmd);
		if (req->gcl != NULL)
		{
			ep_dbg_printf(" [gcl->refcnt %d]", req->gcl->refcnt);
		}
		if (ep_dbg_test(Dbg, 30))
		{
			ep_dbg_printf(", ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}
		else
		{
			ep_dbg_printf("\n");
		}
	}

	d = &DispatchTable[cmd];
	if (d->func == NULL)
		estat = cmd_not_implemented(req);
	else
		estat = (*d->func)(req);

	if (ep_dbg_test(Dbg, 18))
	{
		char ebuf[200];

		ep_dbg_printf("_gdp_req_dispatch <<< %s",
				_gdp_proto_cmd_name(cmd));
		if (req->gcl != NULL)
			ep_dbg_printf(" [gcl->refcnt %d]", req->gcl->refcnt);
		ep_dbg_printf("\n    %s\n", ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(Dbg, 70))
		{
			ep_dbg_printf("    ");
			_gdp_req_dump(req, ep_dbg_getfile(), GDP_PR_BASIC, 0);
		}
	}

	return estat;
}


/*
**  Advertise our existence (and possibly more!)
*/

EP_STAT
_gdp_advertise(EP_STAT (*func)(gdp_buf_t *, void *, int), void *ctx, int cmd)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_req_t *req;
	gdp_chan_t *chan = _GdpChannel;
	uint32_t reqflags = 0;

	ep_dbg_cprintf(Dbg, 39, "_gdp_advertise(%d):\n", cmd);

	// create a new request and point it at the routing layer
	estat = _gdp_req_new(cmd, NULL, chan, NULL, reqflags, &req);
	EP_STAT_CHECK(estat, goto fail0);
	memcpy(req->pdu->dst, RoutingLayerAddr, sizeof req->pdu->dst);

	// add any additional information to advertisement
	if (func != NULL)
		estat = func(req->pdu->datum->dbuf, ctx, cmd);

	// send the request
	estat = _gdp_req_send(req);

	// there is no reply
	_gdp_req_free(&req);

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
_gdp_advertise_me(int cmd)
{
	return _gdp_advertise(NULL, NULL, cmd);
}
