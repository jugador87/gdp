/* vim: set ai sw=4 sts=4 : */

#include "gdpd.h"
#include "gdpd_physlog.h"
#include "thread_pool.h"

#include <ep/ep_string.h>
#include <ep/ep_hexdump.h>

#include <gdp/gdp_protocol.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>

#include <errno.h>
#include <arpa/inet.h>

/************************  PRIVATE  ************************/

static EP_DBG Dbg = EP_DBG_INIT("gdp.gdpd", "GDP Daemon");

static struct event_base *GdpIoEventBase;
static struct thread_pool *cpu_job_thread_pool;

/*
 **  A handle on an open connection.
 */

typedef struct
{
	int dummy;	    // unused
} conn_t;

EP_STAT
implement_me(char *s)
{
	ep_app_error("Not implemented: %s", s);
	return GDP_STAT_NOT_IMPLEMENTED;
}

/*
 **  GDP_GCL_MSG_PRINT --- print a message (for debugging)
 */

void
gcl_msg_print(const gdp_msg_t *msg, FILE *fp)
{
	int i;

	fprintf(fp, "GCL Message %ld, len %zu", msg->msgno, msg->len);
	if (msg->ts.stamp.tv_sec != TT_NOTIME)
	{
		fprintf(fp, ", timestamp ");
		tt_print_interval(&msg->ts, fp, true);
	}
	else
	{
		fprintf(fp, ", no timestamp");
	}
	i = msg->len;
	fprintf(fp, "\n  %s%.*s%s\n", EpChar->lquote, i, msg->data, EpChar->rquote);
}

/*
 **  GDPD_GCL_ERROR --- helper routine for returning errors
 */

EP_STAT
gdpd_gcl_error(gcl_name_t gcl_name, char *msg, EP_STAT logstat, int nak)
{
	gcl_pname_t pname;

	gdp_gcl_printable_name(gcl_name, pname);
	gdp_log(logstat, "%s: %s", msg, pname);
	return GDP_STAT_FROM_NAK(GDP_NAK_C_BADREQ);
}

/*
 **  Command implementations
 */

EP_STAT
cmd_create(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt)
{
	EP_STAT estat;
	gcl_handle_t *gclh;

	estat = gcl_create(cpkt->gcl_name, &gclh);
	if (EP_STAT_ISOK(estat))
	{
		// cache the open GCL Handle for possible future use
		EP_ASSERT_INSIST(!gdp_gcl_name_is_zero(gclh->gcl_name));
		gdp_gcl_cache_add(gclh, GDP_MODE_AO);

		// pass the name back to the caller
		memcpy(rpkt->gcl_name, gclh->gcl_name, sizeof rpkt->gcl_name);
	}
	return estat;
}

EP_STAT
cmd_open_xx(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt, int iomode)
{
	EP_STAT estat;
	gcl_handle_t *gclh;

	gclh = gdp_gcl_cache_get(cpkt->gcl_name, iomode);
	if (gclh != NULL)
	{
		if (ep_dbg_test(Dbg, 12))
		{
			gcl_pname_t pname;

			gdp_gcl_printable_name(cpkt->gcl_name, pname);
			ep_dbg_printf("cmd_open_xx: using cached handle for %s\n", pname);
		}
		rewind(gclh->fp);	// make sure we can switch modes (read/write)
		return EP_STAT_OK;
	}
	if (ep_dbg_test(Dbg, 12))
	{
		gcl_pname_t pname;

		gdp_gcl_printable_name(cpkt->gcl_name, pname);
		ep_dbg_printf("cmd_open_xx: doing initial open for %s\n", pname);
	}
	estat = gcl_open(cpkt->gcl_name, iomode, &gclh);
	if (EP_STAT_ISOK(estat))
		gdp_gcl_cache_add(gclh, iomode);
	return estat;
}

EP_STAT
cmd_open_ao(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt)
{
	return cmd_open_xx(bev, c, cpkt, rpkt, GDP_MODE_AO);
}

EP_STAT
cmd_open_ro(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt)
{
	return cmd_open_xx(bev, c, cpkt, rpkt, GDP_MODE_RO);
}

EP_STAT
cmd_close(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt)
{
	gcl_handle_t *gclh;

	gclh = gdp_gcl_cache_get(cpkt->gcl_name, GDP_MODE_ANY);
	if (gclh == NULL)
	{
		return gdpd_gcl_error(cpkt->gcl_name, "cmd_close: GCL not open",
		GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
	}
	return gcl_close(gclh);
}

EP_STAT
cmd_read(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt)
{
	gcl_handle_t *gclh;
	gdp_msg_t msg;
	EP_STAT estat;

	gclh = gdp_gcl_cache_get(cpkt->gcl_name, GDP_MODE_RO);
	if (gclh == NULL)
	{
		return gdpd_gcl_error(cpkt->gcl_name, "cmd_read: GCL not open",
		GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
	}

	estat = gcl_read(gclh, cpkt->msgno, &msg, gclh->revb);
	EP_STAT_CHECK(estat, goto fail0);
	rpkt->dlen = EP_STAT_TO_LONG(estat);
	rpkt->ts = msg.ts;

	fail0: if (EP_STAT_IS_SAME(estat, EP_STAT_END_OF_FILE))
		rpkt->cmd = GDP_NAK_C_NOTFOUND;
	return estat;
}

EP_STAT
cmd_publish(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt)
{
	gcl_handle_t *gclh;
	char *dp;
	char dbuf[1024];
	gdp_msg_t msg;
	EP_STAT estat;
	int i;

	gclh = gdp_gcl_cache_get(cpkt->gcl_name, GDP_MODE_AO);
	if (gclh == NULL)
	{
		return gdpd_gcl_error(cpkt->gcl_name, "cmd_publish: GCL not open",
		GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
	}

	if (cpkt->dlen > sizeof dbuf)
		dp = ep_mem_malloc(cpkt->dlen);
	else
		dp = dbuf;

	// read in the data from the bufferevent
	i = evbuffer_remove(bufferevent_get_input(bev), dp, cpkt->dlen);
	if (i < cpkt->dlen)
	{
		return gdpd_gcl_error(cpkt->gcl_name, "cmd_publish: short read",
		GDP_STAT_SHORTMSG, GDP_NAK_S_INTERNAL);
	}

	// create the message
	memset(&msg, 0, sizeof msg);
	msg.len = cpkt->dlen;
	msg.data = dp;
	msg.msgno = cpkt->msgno;
	memcpy(&msg.ts, &cpkt->ts, sizeof msg.ts);

	estat = gcl_append(gclh, &msg);

	if (dp != dbuf)
		ep_mem_free(dp);

	// fill in return information
	rpkt->msgno = msg.msgno;
	return estat;
}

EP_STAT
cmd_subscribe(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt)
{
	gcl_handle_t *gclh;

	gclh = gdp_gcl_cache_get(cpkt->gcl_name, GDP_MODE_RO);
	if (gclh == NULL)
	{
		return gdpd_gcl_error(cpkt->gcl_name, "cmd_subscribe: GCL not open",
		GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
	}
	return implement_me("cmd_subscribe");
}

EP_STAT
cmd_not_implemented(struct bufferevent *bev, conn_t *c, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt)
{
	//XXX print/log something here?

	rpkt->cmd = GDP_NAK_S_INTERNAL;
	gdp_pkt_out_hard(rpkt, bufferevent_get_output(bev));
	return GDP_STAT_NOT_IMPLEMENTED;
}

typedef EP_STAT
cmdfunc_t(struct bufferevent *bev, conn_t *conn, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt);

typedef struct
{
	cmdfunc_t *func;		// function to call
	uint8_t ok_stat;	// default OK ack/nak status
} dispatch_ent_t;

/*
 **  Dispatch Table
 **
 **	One per possible command/ack/nak.
 */

#define NOENT	    { NULL, 0 }

dispatch_ent_t DispatchTable[256] =
{
	NOENT,  // 0
	NOENT,                   // 1
	NOENT,                   // 2
	NOENT,                   // 3
	NOENT,                   // 4
	NOENT,                   // 5
	NOENT,                   // 6
	NOENT,                   // 7
	NOENT,                   // 8
	NOENT,                   // 9
	NOENT,                   // 10
	NOENT,                   // 11
	NOENT,                   // 12
	NOENT,                   // 13
	NOENT,                   // 14
	NOENT,                   // 15
	NOENT,                   // 16
	NOENT,                   // 17
	NOENT,                   // 18
	NOENT,                   // 19
	NOENT,                   // 20
	NOENT,                   // 21
	NOENT,                   // 22
	NOENT,                   // 23
	NOENT,                   // 24
	NOENT,                   // 25
	NOENT,                   // 26
	NOENT,                   // 27
	NOENT,                   // 28
	NOENT,                   // 29
	NOENT,                   // 30
	NOENT,                   // 31
	NOENT,                   // 32
	NOENT,                   // 33
	NOENT,                   // 34
	NOENT,                   // 35
	NOENT,                   // 36
	NOENT,                   // 37
	NOENT,                   // 38
	NOENT,                   // 39
	NOENT,                   // 40
	NOENT,                   // 41
	NOENT,                   // 42
	NOENT,                   // 43
	NOENT,                   // 44
	NOENT,                   // 45
	NOENT,                   // 46
	NOENT,                   // 47
	NOENT,                   // 48
	NOENT,                   // 49
	NOENT,                   // 50
	NOENT,                   // 51
	NOENT,                   // 52
	NOENT,                   // 53
	NOENT,                   // 54
	NOENT,                   // 55
	NOENT,                   // 56
	NOENT,                   // 57
	NOENT,                   // 58
	NOENT,                   // 59
	NOENT,                   // 60
	NOENT,                   // 61
	NOENT,                   // 62
	NOENT,                   // 63
	NOENT,                   // 64
	NOENT,                   // 65
	{ cmd_create, GDP_ACK_DATA_CREATED },		// 66
	{ cmd_open_ao, GDP_ACK_SUCCESS },		// 67
	{ cmd_open_ro, GDP_ACK_SUCCESS },		// 68
	{ cmd_close, GDP_ACK_SUCCESS },		// 69
	{ cmd_read, GDP_ACK_DATA_CONTENT },		// 70
	{ cmd_publish, GDP_ACK_DATA_CREATED },		// 71
	{ cmd_subscribe, GDP_ACK_SUCCESS },		// 72
	NOENT,                   // 73
	NOENT,                   // 74
	NOENT,                   // 75
	NOENT,                   // 76
	NOENT,                   // 77
	NOENT,                   // 78
	NOENT,                   // 79
	NOENT,                   // 80
	NOENT,                   // 81
	NOENT,                   // 82
	NOENT,                   // 83
	NOENT,                   // 84
	NOENT,                   // 85
	NOENT,                   // 86
	NOENT,                   // 87
	NOENT,                   // 88
	NOENT,                   // 89
	NOENT,                   // 90
	NOENT,                   // 91
	NOENT,                   // 92
	NOENT,                   // 93
	NOENT,                   // 94
	NOENT,                   // 95
	NOENT,                   // 96
	NOENT,                   // 97
	NOENT,                   // 98
	NOENT,                   // 99
	NOENT,                   // 100
	NOENT,                   // 101
	NOENT,                   // 102
	NOENT,                   // 103
	NOENT,                   // 104
	NOENT,                   // 105
	NOENT,                   // 106
	NOENT,                   // 107
	NOENT,                   // 108
	NOENT,                   // 109
	NOENT,                   // 110
	NOENT,                   // 111
	NOENT,                   // 112
	NOENT,                   // 113
	NOENT,                   // 114
	NOENT,                   // 115
	NOENT,                   // 116
	NOENT,                   // 117
	NOENT,                   // 118
	NOENT,                   // 119
	NOENT,                   // 120
	NOENT,                   // 121
	NOENT,                   // 122
	NOENT,                   // 123
	NOENT,                   // 124
	NOENT,                   // 125
	NOENT,                   // 126
	NOENT,                   // 127
	NOENT,                   // 128
	NOENT,                   // 129
	NOENT,                   // 130
	NOENT,                   // 131
	NOENT,                   // 132
	NOENT,                   // 133
	NOENT,                   // 134
	NOENT,                   // 135
	NOENT,                   // 136
	NOENT,                   // 137
	NOENT,                   // 138
	NOENT,                   // 139
	NOENT,                   // 140
	NOENT,                   // 141
	NOENT,                   // 142
	NOENT,                   // 143
	NOENT,                   // 144
	NOENT,                   // 145
	NOENT,                   // 146
	NOENT,                   // 147
	NOENT,                   // 148
	NOENT,                   // 149
	NOENT,                   // 150
	NOENT,                   // 151
	NOENT,                   // 152
	NOENT,                   // 153
	NOENT,                   // 154
	NOENT,                   // 155
	NOENT,                   // 156
	NOENT,                   // 157
	NOENT,                   // 158
	NOENT,                   // 159
	NOENT,                   // 160
	NOENT,                   // 161
	NOENT,                   // 162
	NOENT,                   // 163
	NOENT,                   // 164
	NOENT,                   // 165
	NOENT,                   // 166
	NOENT,                   // 167
	NOENT,                   // 168
	NOENT,                   // 169
	NOENT,                   // 170
	NOENT,                   // 171
	NOENT,                   // 172
	NOENT,                   // 173
	NOENT,                   // 174
	NOENT,                   // 175
	NOENT,                   // 176
	NOENT,                   // 177
	NOENT,                   // 178
	NOENT,                   // 179
	NOENT,                   // 180
	NOENT,                   // 181
	NOENT,                   // 182
	NOENT,                   // 183
	NOENT,                   // 184
	NOENT,                   // 185
	NOENT,                   // 186
	NOENT,                   // 187
	NOENT,                   // 188
	NOENT,                   // 189
	NOENT,                   // 190
	NOENT,                   // 191
	NOENT,                   // 192
	NOENT,                   // 193
	NOENT,                   // 194
	NOENT,                   // 195
	NOENT,                   // 196
	NOENT,                   // 197
	NOENT,                   // 198
	NOENT,                   // 199
	NOENT,                   // 200
	NOENT,                   // 201
	NOENT,                   // 202
	NOENT,                   // 203
	NOENT,                   // 204
	NOENT,                   // 205
	NOENT,                   // 206
	NOENT,                   // 207
	NOENT,                   // 208
	NOENT,                   // 209
	NOENT,                   // 210
	NOENT,                   // 211
	NOENT,                   // 212
	NOENT,                   // 213
	NOENT,                   // 214
	NOENT,                   // 215
	NOENT,                   // 216
	NOENT,                   // 217
	NOENT,                   // 218
	NOENT,                   // 219
	NOENT,                   // 220
	NOENT,                   // 221
	NOENT,                   // 222
	NOENT,                   // 223
	NOENT,                   // 224
	NOENT,                   // 225
	NOENT,                   // 226
	NOENT,                   // 227
	NOENT,                   // 228
	NOENT,                   // 229
	NOENT,                   // 230
	NOENT,                   // 231
	NOENT,                   // 232
	NOENT,                   // 233
	NOENT,                   // 234
	NOENT,                   // 235
	NOENT,                   // 236
	NOENT,                   // 237
	NOENT,                   // 238
	NOENT,                   // 239
	NOENT,                   // 240
	NOENT,                   // 241
	NOENT,                   // 242
	NOENT,                   // 243
	NOENT,                   // 244
	NOENT,                   // 245
	NOENT,                   // 246
	NOENT,                   // 247
	NOENT,                   // 248
	NOENT,                   // 249
	NOENT,                   // 250
	NOENT,                   // 251
	NOENT,                   // 252
	NOENT,                   // 253
	NOENT,                   // 254
	NOENT,                   // 255
};

/*
 **  DISPATCHCMD --- dispatch a command via the DispatchTable
 **
 **  Parameters:
 **	d --- a pointer to the dispatch table entry
 **	conn --- the connection handle
 **	cpkt --- the command packet
 **	rpkt --- filled in with a response packet
 */

EP_STAT
dispatch_cmd(dispatch_ent_t *d, conn_t *conn, gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt, struct bufferevent *bev)
{
	EP_STAT estat;

	ep_dbg_cprintf(Dbg, 18, "dispatch_cmd: >>> command %d\n", cpkt->cmd);

	// set up response packet
	//	    XXX since we copy almost everything, should we just do it
	//		and skip the memset?
	memcpy(rpkt, cpkt, sizeof *rpkt);
	rpkt->cmd = d->ok_stat;
	rpkt->dlen = 0;

	if (d->func == NULL)
		estat = cmd_not_implemented(bev, conn, cpkt, rpkt);
	else
		estat = (*d->func)(bev, conn, cpkt, rpkt);

	if (!EP_STAT_ISOK(estat) && rpkt->cmd < GDP_NAK_MIN)
	{
		// function didn't specify return code; take a guess ourselves
//	if (EP_STAT_REGISTRY(estat) == EP_REGISTRY_UCB &&
//		EP_STAT_MODULE(estat) == GDP_MODULE &&
//		EP_STAT_DETAIL(estat) >= GDP_ACK_MIN &&
//		EP_STAT_DETAIL(estat) <= GDP_NAK_MAX)
//	    rpkt->cmd = EP_STAT_DETAIL(estat);
//	else
		rpkt->cmd = GDP_NAK_S_INTERNAL;
	}

	if (ep_dbg_test(Dbg, 18))
	{
		char ebuf[200];

		ep_dbg_printf("dispatch_cmd: <<< %d, status %d (%s)\n", cpkt->cmd,
		        rpkt->cmd, ep_stat_tostr(estat, ebuf, sizeof ebuf));
		if (ep_dbg_test(Dbg, 30))
			gdp_pkt_dump_hdr(rpkt, ep_dbg_getfile());
	}
	return estat;
}

void
lev_read_cb_continue(void *continue_data)
{
	lev_read_cb_continue_data *cdata = (lev_read_cb_continue_data *)continue_data;
	gdp_pkt_hdr_t rpktbuf;
	dispatch_ent_t *d;
	struct evbuffer *evb = NULL;

	// got the packet, dispatch it based on the command
	d = &DispatchTable[cdata->cpktbuf->cmd];
	cdata->estat = dispatch_cmd(d, cdata->ctx, cdata->cpktbuf, &rpktbuf, cdata->bev);
	//XXX anything to do with estat here?

	free(cdata->cpktbuf);

	// find the GCL handle, if any
	{
		gcl_handle_t *gclh;

		gclh = gdp_gcl_cache_get(rpktbuf.gcl_name, 0);
		if (gclh != NULL)
		{
			evb = gclh->revb;
			if (ep_dbg_test(Dbg, 40))
			{
				gcl_pname_t pname;

				gdp_gcl_printable_name(gclh->gcl_name, pname);
				ep_dbg_printf("lev_read_cb: using evb %p from %s\n", evb,
				        pname);
			}
		}
	}

	// see if we have any return data
	rpktbuf.dlen = (evb == NULL ? 0 : evbuffer_get_length(evb));

	ep_dbg_cprintf(Dbg, 41, "lev_read_cb: sending %d bytes\n", rpktbuf.dlen);

	// make sure nothing sneaks in...
	if (rpktbuf.dlen > 0)
		evbuffer_lock(evb);

	// send the response packet header
	cdata->estat = gdp_pkt_out(&rpktbuf, bufferevent_get_output(cdata->bev));
	//XXX anything to do with estat here?

	// copy any data
	if (rpktbuf.dlen > 0)
	{
		// still experimental
		//evbuffer_add_buffer_reference(bufferevent_get_output(cdata->bev), evb);

		// slower, but works
		int left = rpktbuf.dlen;

		while (left > 0)
		{
			uint8_t buf[1024];
			int len = left;
			int i;

			if (len > sizeof buf)
				len = sizeof buf;
			i = evbuffer_remove(evb, buf, len);
			if (i < len)
			{
				ep_dbg_cprintf(Dbg, 2,
				        "lev_read_cb: short read (wanted %d, got %d)\n", len, i);
				cdata->estat = GDP_STAT_SHORTMSG;
			}
			if (i <= 0)
				break;
			evbuffer_add(bufferevent_get_output(cdata->bev), buf, i);
			if (ep_dbg_test(Dbg, 43))
			{
				ep_hexdump(buf, i, ep_dbg_getfile(), 0);
			}
			left -= i;
		}
	}

	// we can now unlock
	if (rpktbuf.dlen > 0)
		evbuffer_unlock(evb);
}

/*
 **  LEV_READ_CB --- handle reads on command sockets
 */

void
lev_read_cb(struct bufferevent *bev, void *ctx)
{
	EP_STAT estat;
	gdp_pkt_hdr_t *cpktbuf = malloc(sizeof(gdp_pkt_hdr_t));

	estat = gdp_pkt_in(cpktbuf, bufferevent_get_input(bev));
	if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
		return;

	lev_read_cb_continue_data *data = malloc(sizeof(lev_read_cb_continue_data));
	data->bev = bev;
	data->cpktbuf = cpktbuf;
	data->ctx = ctx;
	data->estat = estat;
	thread_pool_job *new_job = thread_pool_job_new(&lev_read_cb_continue, data);

	thread_pool_add_job(cpu_job_thread_pool, new_job);
}

/*
 **  LEV_EVENT_CB --- handle special events on socket
 */

void
lev_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
	{
		EP_STAT estat = ep_stat_from_errno(errno);
		int err = EVUTIL_SOCKET_ERROR();

		gdp_log(estat, "error on command socket: %d (%s)", err,
		        evutil_socket_error_to_string(err));
	}

	if (EP_UT_BITSET(BEV_EVENT_ERROR | BEV_EVENT_EOF, events))
		bufferevent_free(bev);
}

/*
 **  ACCEPT_CB --- called when a new connection is accepted
 */

void
accept_cb(struct evconnlistener *lev,
evutil_socket_t sockfd, struct sockaddr *sa, int salen, void *ctx)
{
	struct event_base *evbase = GdpIoEventBase;
	struct bufferevent *bev = bufferevent_socket_new(evbase, sockfd,
	        BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
	union sockaddr_xx saddr;
	socklen_t slen = sizeof saddr;

	if (bev == NULL)
	{
		gdp_log(ep_stat_from_errno(errno),
		        "accept_cb: could not allocate bufferevent");
		return;
	}

	if (getpeername(sockfd, &saddr.sa, &slen) < 0)
	{
		gdp_log(ep_stat_from_errno(errno),
		        "accept_cb: connection from unknown peer");
	}
	else if (ep_dbg_test(Dbg, 20))
	{
		char abuf[INET6_ADDRSTRLEN];

		switch (saddr.sa.sa_family)
		{
		case AF_INET:
			inet_ntop(saddr.sa.sa_family, &saddr.sin.sin_addr, abuf,
			        sizeof abuf);
			break;
		case AF_INET6:
			inet_ntop(saddr.sa.sa_family, &saddr.sin6.sin6_addr, abuf,
			        sizeof abuf);
			break;
		default:
			strcpy("<unknown>", abuf);
			break;
		}
		ep_dbg_printf("accept_cb: connection from %s\n", abuf);
	}

	bufferevent_setcb(bev, lev_read_cb, NULL, lev_event_cb, NULL);
	bufferevent_enable(bev, EV_READ | EV_WRITE);
}

/*
 **  LISTENER_ERROR_CB --- called if there is an error when listening
 */

void
listener_error_cb(struct evconnlistener *lev, void *ctx)
{
	struct event_base *evbase = evconnlistener_get_base(lev);
	int err = EVUTIL_SOCKET_ERROR();
	EP_STAT estat;

	estat = ep_stat_from_errno(errno);
	gdp_log(estat, "listener error %d (%s)", err,
	        evutil_socket_error_to_string(err));
	event_base_loopexit(evbase, NULL);
}

/*
 **  GDP_INIT --- initialize this library
 **
 **	We intentionally do not call gdp_init() because that opens
 **	an outgoing socket, which we're not (yet) using.  We do set
 **	up GdpEventBase, but with different options.
 */

static EP_STAT
init_error(const char *msg)
{
	int eno = errno;
	EP_STAT estat = ep_stat_from_errno(eno);

	gdp_log(estat, "gdpd_init: %s", msg);
	ep_app_error("gdpd_init: %s: %s", msg, strerror(eno));
	return estat;
}

EP_STAT
gdpd_init(int listenport)
{
	EP_STAT estat = EP_STAT_OK;
	struct evconnlistener *lev;

	if (evthread_use_pthreads() < 0)
	{
		estat = init_error("evthread_use_pthreads failed");
		goto fail0;
	}

	if (GdpEventBase == NULL)
	{
		// Initialize the EVENT library
		struct event_config *ev_cfg = event_config_new();

		event_config_require_features(ev_cfg, EV_FEATURE_FDS);
		GdpEventBase = event_base_new_with_config(ev_cfg);
		if (GdpEventBase == NULL)
		{
			estat = ep_stat_from_errno(errno);
			ep_app_error("gdpd_init: could not create GdpEventBase: %s",
			        strerror(errno));
		}
		event_config_free(ev_cfg);
	}

	if (GdpIoEventBase == NULL)
	{
		GdpIoEventBase = event_base_new();
		if (GdpIoEventBase == NULL)
		{
			estat = ep_stat_from_errno(errno);
			ep_app_error("gdpd_init: could not GdpIoEventBase");
		}
	}

	gdp_gcl_cache_init();

	// set up the incoming evconnlistener
	if (listenport <= 0)
		listenport = ep_adm_getintparam("swarm.gdp.controlport",
		GDP_PORT_DEFAULT);
	{
		union sockaddr_xx saddr;

		saddr.sin.sin_family = AF_INET;
		saddr.sin.sin_addr.s_addr = INADDR_ANY;
		saddr.sin.sin_port = htons(listenport);
		lev = evconnlistener_new_bind(GdpEventBase, accept_cb,
		        NULL,	    // context
		        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | LEV_OPT_THREADSAFE,
		        -1, &saddr.sa, sizeof saddr.sin);
	}
	if (lev == NULL)
		estat = init_error("gdpd_init: could not create evconnlistener");
	else
		evconnlistener_set_error_cb(lev, listener_error_cb);
	EP_STAT_CHECK(estat, goto fail0);

	// create a thread to run the event loop
//    estat = _gdp_start_event_loop_thread(GdpEventBase);
//    EP_STAT_CHECK(estat, goto fail0);

	// success!
	ep_dbg_cprintf(Dbg, 1, "gdpd_init: listening on port %d\n", listenport);
	return estat;

	fail0:
	{
		char ebuf[200];

		ep_dbg_cprintf(Dbg, 1, "gdpd_init: failed with stat %s\n",
		        ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	return estat;
}

int
main(int argc, char **argv)
{
	int opt;
	int listenport = -1;
	bool run_in_foreground = false;
	EP_STAT estat;

	while ((opt = getopt(argc, argv, "D:FP:")) > 0)
	{
		switch (opt)
		{
		case 'D':
			run_in_foreground = true;
			ep_dbg_set(optarg);
			break;

		case 'F':
			run_in_foreground = true;
			break;

		case 'P':
			listenport = atoi(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	// initialize GDP and the EVENT library
	estat = gdpd_init(listenport);
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_app_abort("Cannot initialize gdp library: %s",
		        ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}
	estat = gcl_physlog_init();
	if (!EP_STAT_ISOK(estat))
	{
		char ebuf[100];

		ep_app_abort("Cannot initialize gcl physlog: %s",
		        ep_stat_tostr(estat, ebuf, sizeof ebuf));
	}

	cpu_job_thread_pool = thread_pool_new(NUM_CPU_CORES);
	thread_pool_init(cpu_job_thread_pool);

	// need to manually run the event loop
	_gdp_start_io_event_loop_thread(GdpIoEventBase);
	_gdp_start_accept_event_loop_thread(GdpEventBase);

	// should never get here
	pthread_join(IoEventLoopThread, NULL);
	ep_app_abort("Fell out of event loop");
}
