/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <gdp/gdp.h>
#include <gdp/gdp_log.h>
#include <gdp/gdp_stat.h>
#include <gdp/gdp_protocol.h>
#include <gdp/gdp_priv.h>
#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>
#include <ep/ep_prflags.h>
#include <ep/ep_string.h>
#include <pthread.h>
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
**	This implements the API to C-based applications.  It is essentially
**	just a pass-through to the gdpd, the GDP daemon.
**
**	In the future this may need to be extended to have knowledge of
**	TSN/AVB, but for now we don't worry about that.
*/

/************************  PRIVATE	************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl", "GCL interface to GDP");


typedef struct gdp_conn_ctx
{
	const char			*peername;
//	  rid_info_t				*rids;
} conn_t;


struct bufferevent	*GdpPortBufferEvent;		// our primary protocol port


#if 0
/*
**	Request Id to RID info mapping
**
**		This is used so that a RID in a response packet can be
**		associated with the corresponding command, including the
**		GCL handle.
**
**		The current implementation is quite stupid, just a linear
**		search.	 This should be OK because the vast majority of
**		uses will allocate a RID and immediately deallocate it
**		when the response arrives.
*/

typedef struct rid_info
{
	gdp_rid_t		rid;
	gcl_handle_t	*gclh;
	uint32_t		flags;
	struct rid_info *next;
} rid_info_t;

#define RINFO_PERSISTENT		0x00000001		// RID doesn't go away
#define RINFO_KEEP_MAPPING		0x00000002		// Keep RID this packet only


rid_info_t			*RidFree;			// head of free list

static rid_info_t **
find_rid_mapping(gdp_rid_t rid, conn_t *conn)
{
	rid_info_t **pp;

	for (pp = &conn->rids; *pp != NULL; pp = &(*pp)->next)
	{
		if ((*pp)->rid == rid)
			return pp;
	}
	return NULL;
}


static rid_info_t *
get_rid_info(gdp_rid_t rid, conn_t *conn)
{
	rid_info_t **pp = find_rid_mapping(rid, conn);

	if (pp == NULL || *pp == NULL)
		return NULL;
	return *pp;
}


static rid_info_t *
add_rid_mapping(gdp_rid_t rid, gcl_handle_t *gclh, conn_t *conn)
{
	rid_info_t **pp = find_rid_mapping(rid, conn);
	rid_info_t *t;

	if (pp != NULL)
	{
		// just re-use the existing slot
		(*pp)->gclh = gclh;
		return *pp;
	}

	// lock data structure
	if (RidFree == NULL)
		RidFree = ep_mem_zalloc(sizeof *RidFree);
	t = RidFree;
	RidFree = t->next;
	t->next = conn->rids;
	conn->rids = t;
	// unlock data structure

	return t;
}


static void
drop_rid_mapping(gdp_rid_t rid, conn_t *conn)
{
	rid_info_t **pp = find_rid_mapping(rid, conn);
	rid_info_t *t;

	if (pp == NULL)
		return;
	t = *pp;
	*pp = (*pp)->next;
	t->next = RidFree;
	RidFree = t;
}


static void
drop_all_rid_info(conn_t *conn)
{
	rid_info_t **rifp = &conn->rids;
	rid_info_t *rif;

	while ((rif = *rifp) != NULL)
	{
		if (rif->gclh != NULL)
			gdp_gcl_close(rif->gclh);
		rif->gclh = NULL;
		rifp = &rif->next;
	}
	(*rifp)->next = RidFree;
	RidFree = conn->rids;
	conn->rids = NULL;
}
#endif // 0


/***********************************************************************
**
**	GCL Caching
**		Let's us find the internal representation of the GCL from
**		the name.  These are not really intended for public use,
**		but they are shared with gdpd.
**
**		FIXME This is a very stupid implementation at the moment.
**
**		FIXME Makes no distinction between io modes (we cludge this
**			  by just opening everything for r/w for now)
***********************************************************************/

static EP_HASH		*OpenGCLCache;


EP_STAT
gdp_gcl_cache_init(void)
{
	EP_STAT estat = EP_STAT_OK;

	if (OpenGCLCache == NULL)
	{
		OpenGCLCache = ep_hash_new("OpenGCLCache", NULL, 0);
		if (OpenGCLCache == NULL)
		{
			estat = ep_stat_from_errno(errno);
			gdp_log(estat, "gdp_gcl_cache_init: could not create OpenGCLCache");
			ep_app_abort("gdp_gcl_cache_init: could not create OpenGCLCache");
		}
	}
	return estat;
}


gcl_handle_t *
gdp_gcl_cache_get(gcl_name_t gcl_name, gdp_iomode_t mode)
{
	gcl_handle_t *gclh;

	// see if we have a pointer to this GCL in the cache
	gclh = ep_hash_search(OpenGCLCache, sizeof (gcl_name_t), (void *) gcl_name);
	if (ep_dbg_test(Dbg, 42))
	{
		gcl_pname_t pbuf;

		gdp_gcl_printable_name(gcl_name, pbuf);
		ep_dbg_printf("gdp_gcl_cache_get: %s => %p\n", pbuf, gclh);
	}
	return gclh;
}


void
gdp_gcl_cache_add(gcl_handle_t *gclh, gdp_iomode_t mode)
{
	// sanity checks
	EP_ASSERT_POINTER_VALID(gclh);
	EP_ASSERT_REQUIRE(!gdp_gcl_name_is_zero(gclh->gcl_name));

	// save it in the cache
	(void) ep_hash_insert(OpenGCLCache,
						sizeof (gcl_name_t), &gclh->gcl_name, gclh);
	if (ep_dbg_test(Dbg, 42))
	{
		gcl_pname_t pbuf;

		gdp_gcl_printable_name(gclh->gcl_name, pbuf);
		ep_dbg_printf("gdp_gcl_cache_add: added %s => %p\n", pbuf, gclh);
	}
}


void
gdp_gcl_cache_drop(gcl_name_t gcl_name, gdp_iomode_t mode)
{
	gcl_handle_t *gclh;

	gclh = ep_hash_insert(OpenGCLCache, sizeof (gcl_name_t), gcl_name, NULL);
	if (ep_dbg_test(Dbg, 42))
	{
		gcl_pname_t pbuf;

		gdp_gcl_printable_name(gclh->gcl_name, pbuf);
		ep_dbg_printf("gdp_gcl_cache_drop: dropping %s => %p\n", pbuf, gclh);
	}
}


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
	fprintf(fp, "GCL Message %d, len %zu", msg->msgno, msg->len);
	if (msg->ts.stamp.tv_sec != TT_NOTIME)
	{
		fprintf(fp, ", timestamp ");
		tt_print_interval(&msg->ts, fp, true);
	}
	else
	{
		fprintf(fp, ", no timestamp");
	}
	if (msg->data != NULL)
	{
		int i = msg->len;
		fprintf(fp, "\n	 %s%.*s%s\n", EpChar->lquote, i,
				(char *) msg->data, EpChar->rquote);
	}
}

// make a printable GCL name from a binary version
void
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
}

// make a binary GCL name from a printable version
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


/***********************************************************************
**
**	Protocol processing
**
***********************************************************************/

static EP_STAT
ack_success(gdp_pkt_hdr_t *pkt,
		gcl_handle_t *gclh,
		struct bufferevent *bev,
		struct gdp_conn_ctx *conn)
{
	EP_STAT estat = GDP_STAT_FROM_ACK(pkt->cmd);
	size_t tocopy = pkt->dlen;

	//	If we started with no gcl id, adopt from incoming packet.
	//	This can happen when creating a GCL.
	if (gclh != NULL)
	{
		if (gdp_gcl_name_is_zero(gclh->gcl_name))
			memcpy(gclh->gcl_name, pkt->gcl_name, sizeof gclh->gcl_name);

		gclh->ts = pkt->ts;

		if (gclh->revb != NULL)
		{
			while (tocopy > 0)
			{
				uint8_t xbuf[1024];
				int len = tocopy;
				int i;

				if (len > sizeof xbuf)
					len = sizeof xbuf;
				i = evbuffer_remove(bufferevent_get_input(bev), xbuf, len);
				if (i > 0)
				{
					evbuffer_add(gclh->revb, xbuf, i);
					tocopy -= i;
				}
				else
					break;		// XXX should give some error here
			}
		}
		else
			evbuffer_drain(bufferevent_get_input(bev), tocopy);
	}

	return estat;
}


static EP_STAT
nak_client(gdp_pkt_hdr_t *pkt,
		gcl_handle_t *gclh,
		struct bufferevent *bev,
		struct gdp_conn_ctx *conn)
{
	ep_dbg_cprintf(Dbg, 2, "nak_client: received %d\n", pkt->cmd);
	return GDP_STAT_FROM_NAK(pkt->cmd);
}


static EP_STAT
nak_server(gdp_pkt_hdr_t *pkt,
		gcl_handle_t *gclh,
		struct bufferevent *bev,
		struct gdp_conn_ctx *conn)
{
	ep_dbg_cprintf(Dbg, 2, "nak_server: received %d, gclh=%p\n",
				pkt->cmd, gclh);
	return GDP_STAT_FROM_NAK(pkt->cmd);
}



/*
**	Command/Ack/Nak Dispatch Table
*/

typedef EP_STAT cmdfunc_t(gdp_pkt_hdr_t *pkt,
						gcl_handle_t *gclh,
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


/*
**	GDP_READ_CB --- data is available for reading from gdpd socket
*/

static void
gdp_read_cb(struct bufferevent *bev, void *ctx)
{
	EP_STAT estat;
	gdp_pkt_hdr_t pkt;
	struct evbuffer *ievb = bufferevent_get_input(bev);
	struct gdp_conn_ctx *conn = ctx;
	gcl_handle_t *gclh;
//	  rid_info_t *rif;
	dispatch_ent_t *d;

	ep_dbg_cprintf(Dbg, 50, "gdp_read_cb: fd %d\n", bufferevent_getfd(bev));

	estat = gdp_pkt_in(&pkt, ievb);
	if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
		return;

	d = &DispatchTable[pkt.cmd];
	if (d->func == NULL)
	{
		const char *peer = conn->peername;

		if (peer == NULL)
			peer = "(unknown)";
		// just ignore unknown commands
		ep_dbg_cprintf(Dbg, 1, "gdp_read_cb: Unknown packet cmd %d from %s\n",
				pkt.cmd, peer);
		return;
	}

//	  // find the associated GCL handle based on rid
//	  if (pkt.rid == GDP_PKT_NO_RID)
//	  {
//		gclh = NULL;
//		rif = NULL;
//	  }
//	  else
//	  {
//		rif = get_rid_info(pkt.rid, conn);
//		if (rif == NULL)
//		{
//			// unknown rid
//			return;
//		}
//		gclh = rif->gclh;
//		rif->flags &= ~RINFO_KEEP_MAPPING;
//	  }
	if (EP_UT_BITSET(GDP_PKT_HAS_ID, pkt.flags))
	{
		gclh = gdp_gcl_cache_get(pkt.gcl_name, 0);
		if (gclh == NULL)
		{
			gcl_pname_t pbuf;

			gdp_gcl_printable_name(pkt.gcl_name, pbuf);
			ep_dbg_cprintf(Dbg, 1, "gdp_read_cb: GCL %s has no handle\n", pbuf);
		}
	}
	else
		gclh = NULL;

	estat = (*d->func)(&pkt, gclh, bev, conn);
	if (gclh == NULL || EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
	{
		ep_dbg_cprintf(Dbg, 44, "gdp_read_cb: keep reading\n");
		return;
	}

//	  // if this a response and the RID isn't persistent, we're done with it
//	  if (rif != NULL && pkt.cmd >= GDP_ACK_MIN &&
//			!EP_UT_BITSET(RINFO_PERSISTENT | RINFO_KEEP_MAPPING, rif->flags))
//		drop_rid_mapping(pkt.rid, conn);

	// return our status via the GCL handle
	pthread_mutex_lock(&gclh->mutex);
	gclh->estat = estat;
	gclh->flags |= GCLH_DONE;
	if (ep_dbg_test(Dbg, 44))
	{
		gcl_pname_t pbuf;
		char ebuf[200];

		gdp_gcl_printable_name(gclh->gcl_name, pbuf);
		ep_dbg_printf("gdp_read_cb: returning stat %s\n\tGCL %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf), pbuf);
	}
	pthread_cond_signal(&gclh->cond);
	pthread_mutex_unlock(&gclh->mutex);

	// shouldn't have to use event_base_loop{exit,break} here because
	// event_base_loop should have been called with EVLOOP_ONCE
}


/*
**	GDP_EVENT_CB --- events or errors occur on gdpd socket
*/

static EP_PRFLAGS_DESC	EventWhatFlags[] =
{
	{ BEV_EVENT_READING,		BEV_EVENT_READING,		"BEV_EVENT_READING"		},
	{ BEV_EVENT_WRITING,		BEV_EVENT_WRITING,		"BEV_EVENT_WRITING"		},
	{ BEV_EVENT_EOF,			BEV_EVENT_EOF,			"BEV_EVENT_EOF"			},
	{ BEV_EVENT_ERROR,			BEV_EVENT_ERROR,		"BEV_EVENT_ERROR"		},
	{ BEV_EVENT_TIMEOUT,		BEV_EVENT_TIMEOUT,		"BEV_EVENT_TIMEOUT"		},
	{ BEV_EVENT_CONNECTED,		BEV_EVENT_CONNECTED,	"BEV_EVENT_CONNECTED"	},
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
		struct evbuffer *ievb = bufferevent_get_input(bev);
		size_t l = evbuffer_get_length(ievb);

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

static EP_STAT
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
*/

static pthread_t	EventLoopThread;

void *
gdp_run_event_loop(void *ctx)
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


EP_STAT
_gdp_start_event_loop_thread(struct event_base *evb)
{
	if (pthread_create(&EventLoopThread, NULL, gdp_run_event_loop, evb) != 0)
		return init_error("cannot create event loop thread");
	else
		return EP_STAT_OK;
}


/*
**	GDP_INIT --- initialize this library
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
gdp_init(bool run_event_loop)
{
	static bool inited = false;
	EP_STAT estat = EP_STAT_OK;
	struct bufferevent *bev;
	extern void gdp_stat_init(void);

	if (inited)
		return EP_STAT_OK;
	inited = true;

	ep_dbg_cprintf(Dbg, 4, "gdp_init: initializing\n");

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

	estat = gdp_gcl_cache_init();
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
		estat = _gdp_start_event_loop_thread(GdpEventBase);
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


/*
**	GCL_NEW --- create a new gcl_handle & initialize
**
**	Returns:
**		New gcl_handle if possible
**		NULL otherwise
*/

static EP_STAT
gcl_handle_new(gcl_handle_t **pgclh)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_handle_t *gclh;

	// allocate the memory to hold the gcl_handle
	gclh = ep_mem_zalloc(sizeof *gclh);
	if (gclh == NULL)
		goto fail1;
	pthread_mutex_init(&gclh->mutex, NULL);
	pthread_cond_init(&gclh->cond, NULL);
	gclh->ts.stamp.tv_sec = TT_NOTIME;

	// success
	*pgclh = gclh;
	return estat;

fail1:
	estat = ep_stat_from_errno(errno);
	return estat;
}



/************************  PUBLIC  ************************/

/*
**	GDP_INVOKE --- do a remote invocation to the GDP daemon
**		Probably only for internal use, but we'll expose it anyway.
**		XXX good idea?
*/

static EP_STAT
gdp_invoke(int cmd, gcl_handle_t *gclh, gdp_msg_t *msg)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_pkt_hdr_t pkt;

	EP_ASSERT_POINTER_VALID(gclh);

	ep_dbg_cprintf(Dbg, 43, "gdp_invoke: cmd=%d\n", cmd);

	// initialize packet header
	//	XXX should probably allocate a RID here
	//	XXX for now just use the address of the handle as the RID.
	//		This prevents having multiple commands in flight for a
	//		single handle, but that's OK for now.
	gdp_pkt_hdr_init(&pkt, cmd, (gdp_rid_t) gclh, gclh->gcl_name);

	// register this handle so we can process the results
	gdp_gcl_cache_add(gclh, 0);

	// write the message out
	pthread_mutex_lock(&gclh->mutex);
	gclh->flags &= ~GCLH_DONE;
	pthread_cond_signal(&gclh->cond);
	pthread_mutex_unlock(&gclh->mutex);
	if (msg != NULL)
	{
		pkt.dlen = msg->len;
		pkt.data = msg->data;
		pkt.msgno = msg->msgno;
		pkt.ts = msg->ts;
	}
	estat = gdp_pkt_out(&pkt, bufferevent_get_output(GdpPortBufferEvent));
	EP_STAT_CHECK(estat, goto fail0);

	// run the event loop until we have a result
	ep_dbg_cprintf(Dbg, 37, "gdp_invoke: waiting\n");
	pthread_mutex_lock(&gclh->mutex);
	while (!EP_UT_BITSET(GCLH_DONE, gclh->flags))
	{
		pthread_cond_wait(&gclh->cond, &gclh->mutex);
	}

	//XXX what status will/should we return?
	if (event_base_got_exit(GdpEventBase))
	{
		ep_dbg_cprintf(Dbg, 1, "gdp_invoke: exiting on loopexit\n");
		estat = GDP_STAT_INTERNAL_ERROR;
	}

	if (EP_UT_BITSET(GCLH_DONE, gclh->flags))
	{
		estat = gclh->estat;
		if (msg != NULL)
			msg->ts = gclh->ts;
	}

	// ok, done!
	pthread_mutex_unlock(&gclh->mutex);
fail0:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 12, "gdp_invoke: returning %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}


EP_STAT
gdp_gcl_print(
			const gcl_handle_t *gclh,
			FILE *fp,
			int detail,
			int indent)
{
	gcl_pname_t nbuf;

	if (gclh == NULL)
	{
		fprintf(fp, "GCL: NULL\n");
	}
	else if (gdp_gcl_name_is_zero(gclh->gcl_name))
	{
		fprintf(fp, "GCL: no name\n");
	}
	else
	{
		EP_ASSERT_POINTER_VALID(gclh);

		gdp_gcl_printable_name(gclh->gcl_name, nbuf);
		fprintf(fp, "GCL: %s\n", nbuf);
	}
	return EP_STAT_OK;
}


/*
**	CREATE_GCL_NAME -- create a name for a new GCL
*/

void
gdp_gcl_newname(gcl_name_t np)
{
	evutil_secure_rng_get_bytes(np, sizeof (gcl_name_t));
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
	estat = gcl_handle_new(&gclh);
	EP_STAT_CHECK(estat, goto fail0);

	if (gcl_name == NULL || gdp_gcl_name_is_zero(gcl_name))
		gdp_gcl_newname(gclh->gcl_name);
	else
		memcpy(gclh->gcl_name, gcl_name, sizeof gclh->gcl_name);

	estat = gdp_invoke(GDP_CMD_CREATE, gclh, NULL);

	*pgclh = gclh;
	return estat;

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

	// XXX determine if name exists and is a GCL
	estat = gcl_handle_new(&gclh);
	EP_STAT_CHECK(estat, goto fail1);
	memcpy(gclh->gcl_name, gcl_name, sizeof gclh->gcl_name);
	gclh->iomode = mode;

	estat = gdp_invoke(cmd, gclh, NULL);

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

	if (gclh == NULL)
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
	estat = gdp_invoke(GDP_CMD_CLOSE, gclh, NULL);

	//XXX should probably check status

	// release resources held by this handle
	pthread_mutex_destroy(&gclh->mutex);
	pthread_cond_destroy(&gclh->cond);
//	  free_all_mappings(gclh->rids);
	gdp_gcl_cache_drop(gclh->gcl_name, 0);
	ep_mem_free(gclh);
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

	estat = gdp_invoke(GDP_CMD_PUBLISH, gclh, msg);
	return estat;
}


/*
**	GDP_GCL_READ --- read a message from a GCL
*/

EP_STAT
gdp_gcl_read(gcl_handle_t *gclh,
			long msgno,
			gdp_msg_t *msg,
			struct evbuffer *revb)
{
	EP_STAT estat = EP_STAT_OK;

	EP_ASSERT_POINTER_VALID(gclh);

	msg->msgno = msgno;
	msg->len = 0;
	msg->ts.stamp.tv_sec = TT_NOTIME;
	gclh->revb = revb;
	estat = gdp_invoke(GDP_CMD_READ, gclh, msg);

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
