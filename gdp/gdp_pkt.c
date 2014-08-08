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
#include <event2/buffer.h>
#include <string.h>
#include <sys/errno.h>

EP_DBG	Dbg = EP_DBG_INIT("gdp.pkt", "GDP packet traffic");


#define FIXEDHDRSZ	(1 + 1 + 1 + 1 + 4)		// ver, cmd, flags, reserved1, dlen

/*
**	GDP_PKT_HDR_INIT --- initialize GDP packet structure
**
**	XXX Perhaps this routine should allocate new RIDs as necessary
**		rather than taking them as parameters?
*/

void
gdp_pkt_hdr_init(gdp_pkt_hdr_t *pp,
				int cmd,
				gdp_rid_t rid,
				gcl_name_t gcl_name)
{
	EP_ASSERT_POINTER_VALID(pp);

	// start with an empty slate
	memset(pp, 0, sizeof *pp);

	pp->ver = GDP_PROTO_CUR_VERSION;
	pp->cmd = cmd;
	pp->rid = rid;
	memcpy(pp->gcl_name, gcl_name, sizeof pp->gcl_name);
	pp->msgno = GDP_PKT_NO_RECNO;
}


/*
**	GDP_PKT_OUT --- send a packet to a network buffer
**
**		Outputs packet header.	pp->dlen must be set to the number
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
gdp_pkt_out(gdp_pkt_hdr_t *pp, struct evbuffer *obuf)
{
	EP_STAT estat = EP_STAT_OK;
	uint8_t pbuf[sizeof *pp];		// overkill, but it works
	uint8_t *pbp = pbuf;

	EP_ASSERT_POINTER_VALID(pp);

	if (ep_dbg_test(Dbg, 22))
	{
		ep_dbg_printf("gdp_pkt_out:\n\t");
		gdp_pkt_dump_hdr(pp, ep_dbg_getfile());
	}

	// version number
	if (pp->ver == 0)
		*pbp++ = GDP_PROTO_CUR_VERSION;
	else
		*pbp++ = pp->ver;

	// command
	*pbp++ = pp->cmd;

	// flags
	*pbp++ = 0;						// flags; we'll fill this in later

	// reserved1
	*pbp++ = 0;						// XXX usable for expansion

	// data length
	PUT32(pp->dlen);

	// request id
	if (pp->rid != GDP_PKT_NO_RID)
	{
		pbuf[2] |= GDP_PKT_HAS_RID;
		PUT64(pp->rid);
	}

	// usc name
	if (!gdp_gcl_name_is_zero(pp->gcl_name))
	{
		pbuf[2] |= GDP_PKT_HAS_ID;
		memcpy(pbp, pp->gcl_name, sizeof pp->gcl_name);
		pbp += sizeof pp->gcl_name;
	}

	// record number
	if (pp->msgno != GDP_PKT_NO_RECNO)
	{
		pbuf[2] |= GDP_PKT_HAS_RECNO;
		PUT32(pp->msgno);
	}

	// timestamp
	if (pp->ts.stamp.tv_sec != TT_NOTIME)
	{
		pbuf[2] |= GDP_PKT_HAS_TS;
		PUT64(pp->ts.stamp.tv_sec);
		PUT32(pp->ts.stamp.tv_nsec);
		PUT32(pp->ts.accuracy);
	}

	//XXX pad out to four octets?

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("gdp_pkt_out: sending packet:\n");
		ep_hexdump(pbuf, pbp - pbuf, ep_dbg_getfile(), 0);
	}

	if (evbuffer_add(obuf, pbuf, pbp - pbuf) < 0)
	{
		// couldn't write, bad juju
		ep_dbg_cprintf(Dbg, 1, "gdp_pkt_out: header write failure: %s\n",
				strerror(errno));
		estat = GDP_STAT_PKT_WRITE_FAIL;
	}
	else if (pp->data != NULL && pp->dlen > 0 &&
			evbuffer_add(obuf, pp->data, pp->dlen) < 0)
	{
		// couldn't write data
		ep_dbg_cprintf(Dbg, 1, "gdp_pkt_out: data write failure: %s\n",
				strerror(errno));
		estat = GDP_STAT_PKT_WRITE_FAIL;
	}

	return estat;
}


/*
**	GDP_PKT_OUT_HARD --- same as above, but complain on error
*/

void
gdp_pkt_out_hard(gdp_pkt_hdr_t *pkt, struct evbuffer *evb)
{
	EP_STAT estat = gdp_pkt_out(pkt, evb);

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
gdp_pkt_in(gdp_pkt_hdr_t *pp, struct evbuffer *ibuf)
{
	EP_STAT estat = EP_STAT_OK;
	size_t needed;
	size_t sz;
	uint8_t pbuf[sizeof *pp];		// overkill, but safe
	uint8_t *pbp;

	EP_ASSERT_POINTER_VALID(pp);
	memset((void *) pp, 0, sizeof *pp);

	ep_dbg_cprintf(Dbg, 60, "gdp_pkt_in\n");	// XXX

	// see if the fixed part of the header is all in
	needed = FIXEDHDRSZ;			// ver, cmd, flags, reserved1, dlen

	if (evbuffer_copyout(ibuf, pbuf, needed) < needed)
	{
		// try again after we read more in
		ep_dbg_cprintf(Dbg, 42,
						"gdp_pkt_in: keep reading (have %zd, need %zd)\n",
						evbuffer_get_length(ibuf), needed);
		return GDP_STAT_KEEP_READING;
	}
	pbp = pbuf;
	pp->ver = *pbp++;
	pp->cmd = *pbp++;
	pp->flags = *pbp++;
	pp->reserved1 = *pbp++;
	GET32(pp->dlen)

	// do some initial error checking
	if (pp->ver < GDP_PROTO_MIN_VERSION || pp->ver > GDP_PROTO_CUR_VERSION)
	{
		ep_dbg_cprintf(Dbg, 1, "gdp_pkt_in: version %d out of range (%d-%d)\n",
				pp->ver, GDP_PROTO_MIN_VERSION, GDP_PROTO_CUR_VERSION);
		estat = GDP_STAT_PKT_VERSION_MISMATCH;
	}

	// figure out how much additional data we will need
	if (EP_UT_BITSET(GDP_PKT_HAS_RID, pp->flags))
		needed += 8;				// sizeof pp->rid;
	if (EP_UT_BITSET(GDP_PKT_HAS_ID, pp->flags))
		needed += 32;				// sizeof pp->gcl_name;
	if (EP_UT_BITSET(GDP_PKT_HAS_RECNO, pp->flags))
		needed += 4;				// sizeof pp->msgno;
	if (EP_UT_BITSET(GDP_PKT_HAS_TS, pp->flags))
		needed += 16;				// sizeof pp->ts;
	needed += pp->dlen;

	// see if the entire packet (header + data) is available
	if (evbuffer_get_length(ibuf) < needed)
	{
		// still not enough data
		if (ep_dbg_test(Dbg, 42))
		{
			char xbuf[256];
			size_t s;

			s = evbuffer_get_length(ibuf);
			ep_dbg_printf("gdp_pkt_in: keep reading (have %zd, need %zd)\n",
						s, needed);
			if (s > sizeof xbuf)
				s = sizeof xbuf;
			evbuffer_copyout(ibuf, xbuf, s);
			ep_hexdump(xbuf, s, ep_dbg_getfile(), 0);
		}
		return GDP_STAT_KEEP_READING;
	}

	// now drain the data we have processed
	sz = evbuffer_remove(ibuf, pbuf, needed - pp->dlen);
	if (sz < needed - pp->dlen)
	{
		ep_dbg_cprintf(Dbg, 1,
				"gdp_pkt_in: evbuffer_drain failed, sz = %zu, needed = %zu\n",
				sz, needed);
		estat = GDP_STAT_PKT_READ_FAIL;
		// buffer is now out of sync; not clear if we can continue
	}

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("gdp_pkt_in: read packet header:\n");
		ep_hexdump(pbuf, needed - pp->dlen, ep_dbg_getfile(), 0);
	}

	// Request Id
	if (!EP_UT_BITSET(GDP_PKT_HAS_RID, pp->flags))
		pp->rid = GDP_PKT_NO_RID;
	else
		GET64(pp->rid);

	// GCL name
	if (!EP_UT_BITSET(GDP_PKT_HAS_ID, pp->flags))
		memset(pp->gcl_name, 0, sizeof pp->gcl_name);
	else
	{
		memcpy(pp->gcl_name, pbp, sizeof pp->gcl_name);
		pbp += sizeof pp->gcl_name;
	}

	// record number
	if (!EP_UT_BITSET(GDP_PKT_HAS_RECNO, pp->flags))
		pp->msgno = GDP_PKT_NO_RECNO;
	else
		GET32(pp->msgno)

	// timestamp
	if (!EP_UT_BITSET(GDP_PKT_HAS_TS, pp->flags))
	{
		memset(&pp->ts, 0, sizeof pp->ts);
		pp->ts.stamp.tv_sec = TT_NOTIME;
	}
	else
	{
		GET64(pp->ts.stamp.tv_sec);
		GET32(pp->ts.stamp.tv_nsec);
		GET32(pp->ts.accuracy)
	}

	//XXX soak up any padding bytes?

	// buffer now points at the data block
	if (ep_dbg_test(Dbg, 22))
	{
		ep_dbg_printf("gdp_pkt_in: ");
		gdp_pkt_dump_hdr(pp, ep_dbg_getfile());
	}

	return estat;
}


static EP_PRFLAGS_DESC	PktFlags[] =
{
	{	GDP_PKT_HAS_RID,	GDP_PKT_HAS_RID,	"GDP_PKT_HAS_RID"	},
	{	GDP_PKT_HAS_ID,		GDP_PKT_HAS_ID,		"GDP_PKT_HAS_NAME"	},
	{	GDP_PKT_HAS_RECNO,	GDP_PKT_HAS_RECNO,	"GDP_PKT_HAS_RECNO" },
	{	GDP_PKT_HAS_TS,		GDP_PKT_HAS_TS,		"GDP_PKT_HAS_TS"	},
	{	0,					0,					NULL				}
};

void
gdp_pkt_dump_hdr(gdp_pkt_hdr_t *pp, FILE *fp)
{
	int len = FIXEDHDRSZ;

	fprintf(fp, "Packet @ %p: ver=%d, cmd=%d (%s), r1=%d\n\tflags=",
				pp, pp->ver, pp->cmd, _gdp_proto_cmd_name(pp->cmd),
				pp->reserved1);
	ep_prflags(pp->flags, PktFlags, fp);
	fprintf(fp, "\n\trid=");
	if (pp->rid == GDP_PKT_NO_RID)
		fprintf(fp, "(none)");
	else
	{
		fprintf(fp, "%016llx", pp->rid);
		len += sizeof pp->rid;
	}
	fprintf(fp, ", msgno=");
	if (pp->msgno == GDP_PKT_NO_RECNO)
		fprintf(fp, "(none)");
	else
	{
		fprintf(fp, "%u", pp->msgno);
		len += sizeof pp->msgno;
	}
	fprintf(fp, "\n\tgcl_name=");
	if (gdp_gcl_name_is_zero(pp->gcl_name))
		fprintf(fp, "(none)");
	else
	{
		gcl_pname_t pname;

		gdp_gcl_printable_name(pp->gcl_name, pname);
		fprintf(fp, "%s", pname);
		len += sizeof pp->gcl_name;
	}
	fprintf(fp, "\n\tts=");
	tt_print_interval(&pp->ts, fp, true);
	if (pp->ts.stamp.tv_sec != TT_NOTIME)
		len += sizeof pp->ts;
	fprintf(fp, "\n\tdlen=%u; total header=%d\n", pp->dlen, len);
}
