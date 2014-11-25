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

#include "gdp.h"
#include "gdp_log.h"
#include "gdp_priv.h"
#include "gdp_pkt.h"

#include <event2/event.h>

#include <string.h>
#include <sys/errno.h>

EP_DBG	Dbg = EP_DBG_INIT("gdp.pkt", "GDP packet traffic");

#define FIXEDHDRSZ	(1 + 1 + 1 + 1 + 4)		// ver, cmd, flags, reserved1, dlen


static EP_PRFLAGS_DESC	PktFlags[] =
{
	{	GDP_PKT_HAS_RID,	GDP_PKT_HAS_RID,	"HAS_RID"		},
	{	GDP_PKT_HAS_ID,		GDP_PKT_HAS_ID,		"HAS_NAME"		},
	{	GDP_PKT_HAS_RECNO,	GDP_PKT_HAS_RECNO,	"HAS_RECNO"		},
	{	GDP_PKT_HAS_TS,		GDP_PKT_HAS_TS,		"HAS_TS"		},
	{	0,					0,					NULL			}
};

void
_gdp_pkt_dump(gdp_pkt_t *pkt, FILE *fp)
{
	int len = FIXEDHDRSZ;

	fprintf(fp, "pkt @ %p: v=%d, cmd=%d=%s, r1=%d, rid=%u\n\tflags=",
				pkt, pkt->ver, pkt->cmd, _gdp_proto_cmd_name(pkt->cmd),
				pkt->reserved1, pkt->rid);
	ep_prflags(pkt->flags, PktFlags, fp);
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
	fprintf(fp, "\n\tdatum=%p", pkt->datum);
	if (pkt->datum != NULL)
	{
		fprintf(fp, ", recno=");
		if (pkt->datum->recno == GDP_PKT_NO_RECNO)
			fprintf(fp, "(none)");
		else
		{
			fprintf(fp, "%" PRIgdp_recno, pkt->datum->recno);
			len += sizeof pkt->datum->recno;
		}
		fprintf(fp, ", dbuf=%p, dlen=%zu", pkt->datum->dbuf,
				pkt->datum->dbuf == NULL ? 0 : gdp_buf_getlength(pkt->datum->dbuf));
		fprintf(fp, "\n\tts=");
		ep_time_print(&pkt->datum->ts, fp, true);
		if (EP_TIME_ISVALID(&pkt->datum->ts))
			len += sizeof pkt->datum->ts;
	}
	fprintf(fp, "\n\ttotal header=%d\n", len);
}


/*
**	GDP_PKT_OUT --- send a packet to a network buffer
**
**		Outputs packet, including all the data in the dbuf.
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
_gdp_pkt_out(gdp_pkt_t *pkt, gdp_chan_t *chan)
{
	EP_STAT estat = EP_STAT_OK;
	uint8_t pbuf[_GDP_MAX_PKT_HDR];
	uint8_t *pbp = pbuf;
	size_t dlen;
	struct evbuffer *obuf = bufferevent_get_output(chan->bev);

	EP_ASSERT_POINTER_VALID(pkt);
	EP_ASSERT_POINTER_VALID(pkt->datum);
	EP_ASSERT_POINTER_VALID(pkt->datum->dbuf);

	if (ep_dbg_test(Dbg, 22))
	{
		ep_dbg_printf("gdp_pkt_out (fd = %d):\n\t",
				bufferevent_getfd(chan->bev));
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
	if (pkt->datum != NULL)
		dlen = evbuffer_get_length(pkt->datum->dbuf);
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
	if (pkt->datum != NULL && pkt->datum->recno != GDP_PKT_NO_RECNO)
	{
		pbuf[2] |= GDP_PKT_HAS_RECNO;
		PUT64(pkt->datum->recno);
	}

	// timestamp
	if (pkt->datum != NULL && EP_TIME_ISVALID(&pkt->datum->ts))
	{
		pbuf[2] |= GDP_PKT_HAS_TS;
		PUT64(pkt->datum->ts.tv_sec);
		PUT32(pkt->datum->ts.tv_nsec);
		PUT32(*((uint32_t *) &pkt->datum->ts.tv_accuracy));
	}

	//XXX pad out to four octets?

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("gdp_pkt_out: sending packet:\n");
		ep_hexdump(pbuf, pbp - pbuf, ep_dbg_getfile(), 0);
	}

	evbuffer_lock(obuf);
	if (evbuffer_add(obuf, pbuf, pbp - pbuf) < 0)
	{
		char nbuf[40];

		// couldn't write, bad juju
		strerror_r(errno, nbuf, sizeof nbuf);
		ep_dbg_cprintf(Dbg, 1, "gdp_pkt_out: header write failure: %s", nbuf);
		estat = GDP_STAT_PKT_WRITE_FAIL;
	}
	else if (dlen > 0)
	{
		uint8_t *bp;

		bp = evbuffer_pullup(pkt->datum->dbuf, dlen);
		if (bp == NULL || evbuffer_add(obuf, bp, dlen) < 0)
		{
			char nbuf[40];

			// couldn't write data
			strerror_r(errno, nbuf, sizeof nbuf);
			ep_dbg_cprintf(Dbg, 1, "gdp_pkt_out: data write failure: %s\n", nbuf);
			estat = GDP_STAT_PKT_WRITE_FAIL;
		}
	}
	evbuffer_unlock(obuf);

	return estat;
}


/*
**	GDP_PKT_OUT_HARD --- same as above, but complain on error
*/

void
_gdp_pkt_out_hard(gdp_pkt_t *pkt, gdp_chan_t *chan)
{
	EP_STAT estat = _gdp_pkt_out(pkt, chan);

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
_gdp_pkt_in(gdp_pkt_t *pkt, gdp_chan_t *chan)
{
	EP_STAT estat = EP_STAT_OK;
	uint32_t dlen;
	size_t needed;
	size_t sz;
	uint8_t pbuf[_GDP_MAX_PKT_HDR];
	uint8_t *pbp;
	gdp_buf_t *ibuf;

	EP_ASSERT_POINTER_VALID(pkt);

	ep_dbg_cprintf(Dbg, 60, "gdp_pkt_in\n");	// XXX
	EP_ASSERT(pkt->datum != NULL);
	EP_ASSERT(pkt->datum->dbuf != NULL);
	ibuf = bufferevent_get_input(chan->bev);

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
	GET32(dlen)

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
		needed += 8;				// sizeof pkt->recno;
	if (EP_UT_BITSET(GDP_PKT_HAS_TS, pkt->flags))
		needed += 16;				// sizeof pkt->ts;
	needed += dlen;

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
	sz = gdp_buf_read(ibuf, pbuf, needed - dlen);
	if (sz < needed - dlen)
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
		ep_hexdump(pbuf, needed - dlen, ep_dbg_getfile(), 0);
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


	// record number
	if (!EP_UT_BITSET(GDP_PKT_HAS_RECNO, pkt->flags))
		pkt->datum->recno = GDP_PKT_NO_RECNO;
	else
		GET64(pkt->datum->recno)

	// timestamp
	if (!EP_UT_BITSET(GDP_PKT_HAS_TS, pkt->flags))
	{
		memset(&pkt->datum->ts, 0, sizeof pkt->datum->ts);
		EP_TIME_INVALIDATE(&pkt->datum->ts);
	}
	else
	{
		GET64(pkt->datum->ts.tv_sec);
		GET32(pkt->datum->ts.tv_nsec);
		GET32(*((uint32_t *) &pkt->datum->ts.tv_accuracy));
	}

	//XXX soak up any padding bytes?

	// ibuf now points at the data block
	{
		size_t l;

		ep_dbg_cprintf(Dbg, 40,
				"_gdp_pkt_in: reading %zd more bytes (%zd available)\n",
				dlen, evbuffer_get_length(ibuf));
		l = evbuffer_remove_buffer(ibuf, pkt->datum->dbuf, dlen);
		if (l < dlen)
		{
			// should never happen since we already have all the data in memory
			ep_dbg_cprintf(Dbg, 2,
					"gdp_pkt_in: cannot read all data; wanted %zd, got %zd\n",
					dlen, l);
		}
	}

	if (ep_dbg_test(Dbg, 22))
	{
		char ebuf[200];

		ep_dbg_printf("gdp_pkt_in => %s\n    ",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		_gdp_pkt_dump(pkt, ep_dbg_getfile());
	}

	return estat;
}


/*
**  Active and Free packet queues
**
**		When packets come off the wire they are added to the Active
**		queue; a separate consumer thread consumes them.  Normally it
**		is the I/O thread adding to the queue and the main thread
**		consuming, but this could be changed to have a thread pool
**		doing the consumption.
**
**		The free list is just to avoid memory fragmentation.
**
**		Note that the Active queue is organized as a FIFO, unlike the
**		free lists, which are managed as LIFOs in order to try to keep
**		the hardware cache as hot as possible.
**
**		XXX	Packet queue is not implemented at the moment, implying
**			that processing is single threaded.  This will be needed
**			if we want to do processing out of a thread pool.
*/

static EP_THR_MUTEX		PktFreeListMutex	EP_THR_MUTEX_INITIALIZER;
static TAILQ_HEAD(pkt_head, gdp_pkt)
						PktFreeList = TAILQ_HEAD_INITIALIZER (PktFreeList);

#ifdef GDP_PACKET_QUEUE
static EP_THR_MUTEX		ActivePktMutex		EP_THR_MUTEX_INITIALIZER;
static EP_THR_COND		ActivePktCond		EP_THR_COND_INITIALIZER;
static TAILQ_HEAD(active_head, gdp_pkt)
						ActivePacketQueue = TAILQ_HEAD_INITIALIZER(ActivePacketQueue);
#endif


/*
**  _GDP_PKT_NEW --- allocate a packet (from free list if possible)
**  _GDP_PKT_FREE --- return a packet to the free list
*/

gdp_pkt_t *
_gdp_pkt_new(void)
{
	gdp_pkt_t *pkt;

	ep_thr_mutex_lock(&PktFreeListMutex);
	if ((pkt = TAILQ_FIRST(&PktFreeList)) != NULL)
		TAILQ_REMOVE(&PktFreeList, pkt, list);
	ep_thr_mutex_unlock(&PktFreeListMutex);

	if (pkt == NULL)
	{
		pkt = ep_mem_zalloc(sizeof *pkt);
	}

	// initialize the packet
	EP_ASSERT(!pkt->inuse);
	memset(pkt, 0, sizeof *pkt);
	pkt->ver = GDP_PROTO_CUR_VERSION;
	pkt->datum = gdp_datum_new();
	pkt->inuse = true;

	ep_dbg_cprintf(Dbg, 48, "gdp_pkt_new => %p\n", pkt);
	return pkt;
}

void
_gdp_pkt_free(gdp_pkt_t *pkt)
{
	ep_dbg_cprintf(Dbg, 48, "gdp_pkt_free(%p)\n", pkt);
	EP_ASSERT(pkt->inuse);
	if (pkt->datum != NULL)
		gdp_datum_free(pkt->datum);
	pkt->datum = NULL;
	pkt->inuse = false;
	ep_thr_mutex_lock(&PktFreeListMutex);
	TAILQ_INSERT_HEAD(&PktFreeList, pkt, list);
	ep_thr_mutex_unlock(&PktFreeListMutex);
}


#ifdef GDP_PACKET_QUEUE

/*
**  _GDP_PKT_ADD_TO_QUEUE --- add a packet to the active queue
**  _GDP_PKT_GET_ACTIVE --- get a packet from the active queue
*/

void
_gdp_pkt_add_to_queue(gdp_pkt_t *pkt)
{
	ep_thr_mutex_lock(&ActivePktMutex);
	TAILQ_INSERT_TAIL(&ActivePacketQueue, pkt, list);
	ep_thr_cond_signal(&ActivePktCond);
	ep_thr_mutex_unlock(&ActivePktMutex);
}

gdp_pkt_t *
_gdp_pkt_get_active(void)
{
	gdp_pkt_t *pkt;

	ep_thr_mutex_lock(&ActivePktMutex);
	while (TAILQ_EMPTY(&ActivePacketQueue))
	{
		ep_thr_cond_wait(&ActivePktCond, &ActivePktMutex, NULL);
	}
	pkt = TAILQ_FIRST(&ActivePacketQueue);
	TAILQ_REMOVE(&ActivePacketQueue, pkt, list);
	ep_thr_mutex_unlock(&ActivePktMutex);

	return pkt;
}
#endif
