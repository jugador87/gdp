/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_PKT.C --- low-level packet internal <=> external translations
**
**		Everything is read and written in network byte order.
**
**		XXX Should we be working on raw files instead of stdio files?
**			Will make a difference if we use UDP.
**		XXX Should we pad out the packet header to an even four bytes?
**			Might help some implementations when reading or writing
**			the data.
*/

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_prflags.h>
#include <ep/ep_stat.h>
#include <gdp/gdp.h>
#include <gdp/gdp_log.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_pkt.h>
#include <gdp/gdp_timestamp.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <string.h>
#include <sys/errno.h>

EP_DBG	Dbg = EP_DBG_INIT("gdp.pkt", "GDP packet traffic");

#define FIXEDHDRSZ	(1 + 1 + 1 + 1 + 4)		// ver, cmd, flags, reserved1, dlen


static EP_PRFLAGS_DESC	PktFlags[] =
{
	{	GDP_PKT_HAS_RID,	GDP_PKT_HAS_RID,	"GDP_PKT_HAS_RID"	},
	{	GDP_PKT_HAS_ID,		GDP_PKT_HAS_ID,		"GDP_PKT_HAS_NAME"	},
	{	GDP_PKT_HAS_RECNO,	GDP_PKT_HAS_RECNO,	"GDP_PKT_HAS_RECNO" },
	{	GDP_PKT_HAS_TS,		GDP_PKT_HAS_TS,		"GDP_PKT_HAS_TS"	},
	{	0,					0,					NULL				}
};

void
_gdp_pkt_dump(gdp_pkt_t *pkt, FILE *fp)
{
	int len = FIXEDHDRSZ;

	fprintf(fp, "Packet @ %p: ver=%d, cmd=%d (%s), r1=%d\n\tflags=",
				pkt, pkt->ver, pkt->cmd, _gdp_proto_cmd_name(pkt->cmd),
				pkt->reserved1);
	ep_prflags(pkt->flags, PktFlags, fp);
	fprintf(fp, "\n\trid=");
	if (pkt->rid == GDP_PKT_NO_RID)
		fprintf(fp, "(none)");
	else
	{
		fprintf(fp, "%u", pkt->rid);
		len += sizeof pkt->rid;
	}
	fprintf(fp, ", recno=");
	if (pkt->msg == NULL || pkt->msg->recno == GDP_PKT_NO_RECNO)
		fprintf(fp, "(none)");
	else
	{
		fprintf(fp, "%u", pkt->msg->recno);
		len += sizeof pkt->msg->recno;
	}
	fprintf(fp, ", msg=%p", pkt->msg);
	if (pkt->msg != NULL)
		fprintf(fp, ", dbuf=%p", pkt->msg->dbuf);
	fprintf(fp, "\n\tgcl_name=");
	if (gdp_gcl_name_is_zero(pkt->gcl_name))
		fprintf(fp, "(none)");
	else
	{
		gcl_pname_t pname;

		gdp_gcl_printable_name(pkt->gcl_name, pname);
		fprintf(fp, "%s", pname);
		len += sizeof pkt->gcl_name;
	}
	fprintf(fp, "\n\tts=");
	if (pkt->msg == NULL)
		fprintf(fp, "(none)");
	else
	{
		tt_print_interval(&pkt->msg->ts, fp, true);
		if (pkt->msg->ts.stamp.tv_sec != TT_NOTIME)
			len += sizeof pkt->msg->ts;
	}
	fprintf(fp, "\n\tdlen=%zu; total header=%d\n",
			pkt->msg == NULL ? 0 : gdp_buf_getlength(pkt->msg->dbuf), len);
}


/*
**	GDP_PKT_INIT --- initialize GDP packet structure
*/

void
_gdp_pkt_init(gdp_pkt_t *pkt,
		gdp_msg_t *msg)
{
	EP_ASSERT_POINTER_VALID(pkt);
	EP_ASSERT_POINTER_VALID(msg);

	// start with an empty slate
	memset(pkt, 0, sizeof *pkt);

	pkt->ver = GDP_PROTO_CUR_VERSION;
	pkt->msg = msg;
}

#define CANARY	UINT32_C(0x5A5A5A5A)


/*
**	GDP_PKT_OUT --- send a packet to a network buffer
**
**		Outputs packet header.	pkt->msg->dlen must be set to the number
**		of bytes to be added (after we return) to the output buffer
**		obuf.
**
**		NOTE WELL:	If you are going to be adding output data, it is
**		important that the caller lock the output buffer (using
**		evbuffer_lock/evbuffer_unlock) before calling this routine
**		to ensure that other threads don't sneak in and intermix
**		data.
**
**	XXX need to enable buffer locking somewhere!!
**		[evbuffer_enable_locking(buffer, void *lock)]
**		[see also evthread_set_lock_creation_callback]
*/

#define PUT32(v) \
		{ \
			*pbp++ = ((v) >> 24) & 0xff; \
			*pbp++ = ((v) >> 16) & 0xff; \
			*pbp++ = ((v) >> 8) & 0xff; \
			*pbp++ = ((v) & 0xff); \
		}
#define PUT64(v) \
		{ \
			*pbp++ = ((v) >> 56) & 0xff; \
			*pbp++ = ((v) >> 48) & 0xff; \
			*pbp++ = ((v) >> 40) & 0xff; \
			*pbp++ = ((v) >> 32) & 0xff; \
			*pbp++ = ((v) >> 24) & 0xff; \
			*pbp++ = ((v) >> 16) & 0xff; \
			*pbp++ = ((v) >> 8) & 0xff; \
			*pbp++ = ((v) & 0xff); \
		}

EP_STAT
_gdp_pkt_out(gdp_pkt_t *pkt, gdp_buf_t *obuf)
{
	EP_STAT estat = EP_STAT_OK;
	uint8_t pbuf[_GDP_MAX_PKT_HDR];
	uint8_t *pbp = pbuf;
	size_t dlen;
	uint32_t canary = CANARY;

	EP_ASSERT_POINTER_VALID(pkt);
	EP_ASSERT_POINTER_VALID(pkt->msg);
	EP_ASSERT_POINTER_VALID(pkt->msg->dbuf);

	if (ep_dbg_test(Dbg, 22))
	{
		ep_dbg_printf("gdp_pkt_out:\n\t");
		_gdp_pkt_dump(pkt, ep_dbg_getfile());
	}

	// version number
	if (pkt->ver == 0)
		*pbp++ = GDP_PROTO_CUR_VERSION;
	else
		*pbp++ = pkt->ver;

	// command
	*pbp++ = pkt->cmd;

	// flags
	*pbp++ = 0;						// flags; we'll fill this in later

	// reserved1
	*pbp++ = 0;						// XXX usable for expansion

	// data length
	if (pkt->msg != NULL)
		dlen = gdp_buf_getlength(pkt->msg->dbuf);
	else
		dlen = 0;
	PUT32(dlen);

	// request id
	if (pkt->rid != GDP_PKT_NO_RID)
	{
		pbuf[2] |= GDP_PKT_HAS_RID;
		PUT32(pkt->rid);
	}

	// GCL name
	if (!gdp_gcl_name_is_zero(pkt->gcl_name))
	{
		pbuf[2] |= GDP_PKT_HAS_ID;
		memcpy(pbp, pkt->gcl_name, sizeof pkt->gcl_name);
		pbp += sizeof pkt->gcl_name;
	}

	// record number
	if (pkt->msg != NULL && pkt->msg->recno != GDP_PKT_NO_RECNO)
	{
		pbuf[2] |= GDP_PKT_HAS_RECNO;
		PUT32(pkt->msg->recno);
	}

	// timestamp
	if (pkt->msg != NULL && pkt->msg->ts.stamp.tv_sec != TT_NOTIME)
	{
		pbuf[2] |= GDP_PKT_HAS_TS;
		PUT64(pkt->msg->ts.stamp.tv_sec);
		PUT32(pkt->msg->ts.stamp.tv_nsec);
		PUT32(pkt->msg->ts.accuracy);
	}

	//XXX pad out to four octets?

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("gdp_pkt_out: sending packet:\n");
		ep_hexdump(pbuf, pbp - pbuf, ep_dbg_getfile(), 0);
	}

	size_t written;

//	evbuffer_lock(obuf);
	if (gdp_buf_write(obuf, pbuf, pbp - pbuf) < 0)
	{
		// couldn't write, bad juju
		ep_dbg_cprintf(Dbg, 1, "gdp_pkt_out: header write failure: %s",
				strerror(errno));
		estat = GDP_STAT_PKT_WRITE_FAIL;
	}
	else if (pkt->msg != NULL && pkt->msg->dbuf != NULL &&
			(dlen = gdp_buf_getlength(pkt->msg->dbuf)) > 0 &&
			(written = evbuffer_remove_buffer(pkt->msg->dbuf, obuf, dlen)) < dlen)
	{
		// couldn't write data
		ep_dbg_cprintf(Dbg, 1, "gdp_pkt_out: data write failure: %s\n"
				"  wanted %zd, got %zd\n",
				strerror(errno), gdp_buf_getlength(pkt->msg->dbuf), written);
		estat = GDP_STAT_PKT_WRITE_FAIL;
	}
//	evbuffer_unlock(obuf);
	EP_ASSERT(canary == CANARY);

	return estat;
}


/*
**	GDP_PKT_OUT_HARD --- same as above, but complain on error
*/

void
_gdp_pkt_out_hard(gdp_pkt_t *pkt, gdp_buf_t *obuf)
{
	EP_STAT estat = _gdp_pkt_out(pkt, obuf);

	if (!EP_STAT_ISOK(estat))
	{
		gdp_log(estat, "gdp_pkt_out_hard: cannot put packet");
	}
}


/*
**	GDP_PKT_IN --- read a packet from the network
**
**		The caller has to loop on this while it returns
**		GDP_STAT_KEEP_READING, which means that some required data
**		isn't present yet.
**
**	XXX This can probably done more efficiently using evbuffer_peek.
*/

#define GET32(v) \
		{ \
				v  = *pbp++ << 24; \
				v |= *pbp++ << 16; \
				v |= *pbp++ << 8; \
				v |= *pbp++; \
		}
#define GET64(v) \
		{ \
				v  = ((uint64_t) *pbp++) << 56; \
				v |= ((uint64_t) *pbp++) << 48; \
				v |= ((uint64_t) *pbp++) << 40; \
				v |= ((uint64_t) *pbp++) << 32; \
				v |= ((uint64_t) *pbp++) << 24; \
				v |= ((uint64_t) *pbp++) << 16; \
				v |= ((uint64_t) *pbp++) << 8; \
				v |= ((uint64_t) *pbp++); \
		}

EP_STAT
_gdp_pkt_in(gdp_pkt_t *pkt, gdp_buf_t *ibuf)
{
	EP_STAT estat = EP_STAT_OK;
	size_t needed;
	size_t sz;
	uint8_t pbuf[_GDP_MAX_PKT_HDR];
	uint8_t *pbp;

	EP_ASSERT_POINTER_VALID(pkt);

	ep_dbg_cprintf(Dbg, 60, "gdp_pkt_in\n");	// XXX

	// see if the fixed part of the header is all in
	needed = FIXEDHDRSZ;			// ver, cmd, flags, reserved1, dlen

	if (gdp_buf_peek(ibuf, pbuf, needed) < needed)
	{
		// try again after we read more in
		ep_dbg_cprintf(Dbg, 42,
						"gdp_pkt_in: keep reading (have %zd, need %zd)\n",
						gdp_buf_getlength(ibuf), needed);
		return GDP_STAT_KEEP_READING;
	}

	pbp = pbuf;
	pkt->ver = *pbp++;
	pkt->cmd = *pbp++;
	pkt->flags = *pbp++;
	pkt->reserved1 = *pbp++;
	GET32(pkt->msg->dlen)

	// do some initial error checking
	if (pkt->ver < GDP_PROTO_MIN_VERSION || pkt->ver > GDP_PROTO_CUR_VERSION)
	{
		ep_dbg_cprintf(Dbg, 1, "gdp_pkt_in: version %d out of range (%d-%d)\n",
				pkt->ver, GDP_PROTO_MIN_VERSION, GDP_PROTO_CUR_VERSION);
		estat = GDP_STAT_PKT_VERSION_MISMATCH;
	}

	// figure out how much additional data we will need
	if (EP_UT_BITSET(GDP_PKT_HAS_RID, pkt->flags))
		needed += 4;				// sizeof pkt->rid;
	if (EP_UT_BITSET(GDP_PKT_HAS_ID, pkt->flags))
		needed += 32;				// sizeof pkt->gcl_name;
	if (EP_UT_BITSET(GDP_PKT_HAS_RECNO, pkt->flags))
		needed += 4;				// sizeof pkt->recno;
	if (EP_UT_BITSET(GDP_PKT_HAS_TS, pkt->flags))
		needed += 16;				// sizeof pkt->ts;
	needed += pkt->msg->dlen;

	// see if the entire packet (header + data) is available
	if (gdp_buf_getlength(ibuf) < needed)
	{
		// still not enough data
		if (ep_dbg_test(Dbg, 42))
		{
			char xbuf[256];
			size_t s;

			s = gdp_buf_getlength(ibuf);
			ep_dbg_printf("gdp_pkt_in: keep reading (have %zd, need %zd)\n",
						s, needed);
			if (s > sizeof xbuf)
				s = sizeof xbuf;
			gdp_buf_peek(ibuf, xbuf, s);
			ep_hexdump(xbuf, s, ep_dbg_getfile(), 0);
		}
		return GDP_STAT_KEEP_READING;
	}

	// the entire packet is now in ibuf

	// now drain the data we have processed
	sz = gdp_buf_read(ibuf, pbuf, needed - pkt->msg->dlen);
	if (sz < needed - pkt->msg->dlen)
	{
		// shouldn't happen, since it's already in memory
		estat = GDP_STAT_BUFFER_FAILURE;
		gdp_log(estat,
				"gdp_pkt_in: gdp_buf_drain failed, sz = %zu, needed = %zu\n",
				sz, needed);
		// buffer is now out of sync; not clear if we can continue
	}

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("gdp_pkt_in: read packet header:\n");
		ep_hexdump(pbuf, needed - pkt->msg->dlen, ep_dbg_getfile(), 0);
	}

	// Request Id
	if (!EP_UT_BITSET(GDP_PKT_HAS_RID, pkt->flags))
		pkt->rid = GDP_PKT_NO_RID;
	else
		GET32(pkt->rid);

	// GCL name
	if (!EP_UT_BITSET(GDP_PKT_HAS_ID, pkt->flags))
		memset(pkt->gcl_name, 0, sizeof pkt->gcl_name);
	else
	{
		memcpy(pkt->gcl_name, pbp, sizeof pkt->gcl_name);
		pbp += sizeof pkt->gcl_name;
	}

	if (pkt->msg != NULL)
	{
		// record number
		if (!EP_UT_BITSET(GDP_PKT_HAS_RECNO, pkt->flags))
			pkt->msg->recno = GDP_PKT_NO_RECNO;
		else
			GET32(pkt->msg->recno)

		// timestamp
		if (!EP_UT_BITSET(GDP_PKT_HAS_TS, pkt->flags))
		{
			memset(&pkt->msg->ts, 0, sizeof pkt->msg->ts);
			pkt->msg->ts.stamp.tv_sec = TT_NOTIME;
		}
		else
		{
			GET64(pkt->msg->ts.stamp.tv_sec);
			GET32(pkt->msg->ts.stamp.tv_nsec);
			GET32(pkt->msg->ts.accuracy)
		}
	}

	//XXX soak up any padding bytes?

	// buffer now points at the data block, to be read at higher level

	if (ep_dbg_test(Dbg, 22))
	{
		ep_dbg_printf("gdp_pkt_in: ");
		_gdp_pkt_dump(pkt, ep_dbg_getfile());
	}

	return estat;
}
