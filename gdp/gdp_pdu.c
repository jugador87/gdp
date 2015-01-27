/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_PDU.C --- low-level packet internal <=> external translations
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
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>
#include <ep/ep_stat.h>

#include "gdp.h"
#include "gdp_priv.h"
#include "gdp_pdu.h"

#include <event2/event.h>

#include <string.h>
#include <sys/errno.h>

EP_DBG	Dbg = EP_DBG_INIT("gdp.pdu", "GDP PDU traffic");


static EP_PRFLAGS_DESC	PduFlags[] =
{
	{	GDP_PDU_HAS_RID,	GDP_PDU_HAS_RID,	"HAS_RID"		},
	{	GDP_PDU_HAS_RECNO,	GDP_PDU_HAS_RECNO,	"HAS_RECNO"		},
	{	GDP_PDU_HAS_SEQNO,	GDP_PDU_HAS_SEQNO,	"HAS_SEQNO"		},
	{	GDP_PDU_HAS_TS,		GDP_PDU_HAS_TS,		"HAS_TS"		},
	{	0,					0,					NULL			}
};

void
_gdp_pdu_dump(gdp_pdu_t *pdu, FILE *fp)
{
	fprintf(fp, "PDU@%p: ", pdu);
	if (pdu == NULL)
	{
		fprintf(fp, "NULL\n");
		return;
	}

	int len = _GDP_PDU_FIXEDHDRSZ + pdu->olen;
	fprintf(fp, "v=%d, ttl=%d, cmd=%d=%s,"
				"\n\tolen=%d, rid=%u, sigalg=%x"
				"\n\tflags=",
				pdu->ver, pdu->ttl,
				pdu->cmd, _gdp_proto_cmd_name(pdu->cmd),
				pdu->olen, pdu->rid, pdu->sigalg);
	ep_prflags(pdu->flags, PduFlags, fp);
	fprintf(fp, "\n\tdst=");
	gdp_print_name(pdu->dst, fp);
	fprintf(fp, "\n\tsrc=");
	gdp_print_name(pdu->src, fp);
	fprintf(fp, "\n\tdatum=%p", pdu->datum);
	if (pdu->datum != NULL)
	{
		fprintf(fp, ", recno=");
		if (pdu->datum->recno == GDP_PDU_NO_RECNO)
			fprintf(fp, "(none)");
		else
		{
			fprintf(fp, "%" PRIgdp_recno, pdu->datum->recno);
			len += sizeof pdu->datum->recno;
		}
		fprintf(fp, ", dbuf=%p, dlen=%zu", pdu->datum->dbuf,
				pdu->datum->dbuf == NULL ? 0 : gdp_buf_getlength(pdu->datum->dbuf));
		fprintf(fp, "\n\tts=");
		ep_time_print(&pdu->datum->ts, fp, true);
		if (EP_TIME_ISVALID(&pdu->datum->ts))
			len += sizeof pdu->datum->ts;
	}
	fprintf(fp, "\n\ttotal header=%d\n", len);
}


/*
**	GDP_PDU_OUT --- send a packet to a network buffer
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
#define PUT48(v) \
		{ \
			*pbp++ = ((v) >> 40) & 0xff; \
			*pbp++ = ((v) >> 32) & 0xff; \
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

#define OOFF		70		// ofset of olen from beginning of pdu
#define FOFF		71		// offet of flags from beginning of pdu

EP_STAT
_gdp_pdu_out(gdp_pdu_t *pdu, gdp_chan_t *chan)
{
	EP_STAT estat = EP_STAT_OK;
	uint8_t pbuf[_GDP_PDU_MAXHDRSZ];
	uint8_t *pbp = pbuf;
	size_t dlen;
	size_t hdrlen;
	struct evbuffer *obuf = bufferevent_get_output(chan->bev);

	EP_ASSERT_POINTER_VALID(pdu);
	EP_ASSERT_POINTER_VALID(pdu->datum);
	EP_ASSERT_POINTER_VALID(pdu->datum->dbuf);

	if (ep_dbg_test(Dbg, 22))
	{
		ep_dbg_printf("_gdp_pdu_out (fd = %d):\n\t",
				bufferevent_getfd(chan->bev));
		_gdp_pdu_dump(pdu, ep_dbg_getfile());
	}

	if (!gdp_name_is_valid(pdu->src))
	{
		// use our own name as the source if nothing specified
		memcpy(pdu->src, _GdpMyRoutingName, sizeof pdu->src);
	}

	// version number
	*pbp++ = GDP_PROTO_CUR_VERSION;

	// time to live (in hops)
	*pbp++ = pdu->ttl;

	// reserved field
	*pbp++ = pdu->rsvd1;

	// command
	*pbp++ = pdu->cmd;

	// destination address
	memcpy(pbp, pdu->dst, sizeof pdu->dst);
	pbp += sizeof pdu->dst;

	// source address
	memcpy(pbp, pdu->src, sizeof pdu->src);
	pbp += sizeof pdu->src;

	// signature algorithm and size
	*pbp++ = pdu->sigalg;
	*pbp++ = pdu->siglen;

	// length of options (filled in later)
	*pbp++ = 0;

	// flags (filled in later)
	*pbp++ = 0;

	// data length
	if (pdu->datum != NULL)
		dlen = evbuffer_get_length(pdu->datum->dbuf);
	else
		dlen = 0;
	PUT32(dlen);

	// end of fixed part of header
	EP_ASSERT((pbp - pbuf) == _GDP_PDU_FIXEDHDRSZ);

	// request id
	if (pdu->rid != GDP_PDU_NO_RID)
	{
		pbuf[FOFF] |= GDP_PDU_HAS_RID;
		PUT32(pdu->rid);
	}

	// record number
	if (pdu->datum != NULL && pdu->datum->recno != GDP_PDU_NO_RECNO)
	{
		pbuf[FOFF] |= GDP_PDU_HAS_RECNO;
		PUT64(pdu->datum->recno);
	}

	// sequence number
	if (pdu->seqno > 0)
	{
		pbuf[FOFF] |= GDP_PDU_HAS_SEQNO;
		PUT64(pdu->datum->recno);
	}

	// timestamp
	if (pdu->datum != NULL && EP_TIME_ISVALID(&pdu->datum->ts))
	{
		pbuf[FOFF] |= GDP_PDU_HAS_TS;
		PUT64(pdu->datum->ts.tv_sec);
		PUT32(pdu->datum->ts.tv_nsec);
		PUT32(*((uint32_t *) &pdu->datum->ts.tv_accuracy));
	}

	hdrlen = pbp - pbuf;
	pbuf[OOFF] = ((hdrlen - _GDP_PDU_FIXEDHDRSZ) + 3) / 4;

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("_gdp_pdu_out: sending packet:\n");
		ep_hexdump(pbuf, hdrlen, ep_dbg_getfile(), EP_HEXDUMP_HEX, 0);
	}

	evbuffer_lock(obuf);
	if (evbuffer_add(obuf, pbuf, pbp - pbuf) < 0)
	{
		char nbuf[40];

		// couldn't write, bad juju
		strerror_r(errno, nbuf, sizeof nbuf);
		ep_dbg_cprintf(Dbg, 1, "_gdp_pdu_out: header write failure: %s", nbuf);
		estat = GDP_STAT_PDU_WRITE_FAIL;
	}
	else if (dlen > 0)
	{
		uint8_t *bp;

		bp = evbuffer_pullup(pdu->datum->dbuf, dlen);
		if (bp == NULL || evbuffer_add(obuf, bp, dlen) < 0)
		{
			char nbuf[40];

			// couldn't write data
			strerror_r(errno, nbuf, sizeof nbuf);
			ep_dbg_cprintf(Dbg, 1, "_gdp_pdu_out: data write failure: %s\n", nbuf);
			estat = GDP_STAT_PDU_WRITE_FAIL;
		}
		else if (ep_dbg_test(Dbg, 33))
		{
			ep_hexdump(bp, dlen, ep_dbg_getfile(), EP_HEXDUMP_ASCII,
					pbp - pbuf);
		}
	}

	// XXX output signature here XXX
	evbuffer_unlock(obuf);

	return estat;
}


/*
**	GDP_PDU_OUT_HARD --- same as above, but complain on error
*/

void
_gdp_pdu_out_hard(gdp_pdu_t *pdu, gdp_chan_t *chan)
{
	EP_STAT estat = _gdp_pdu_out(pdu, chan);

	if (!EP_STAT_ISOK(estat))
	{
		ep_log(estat, "_gdp_pdu_out_hard: cannot put packet");
	}
}


/*
**	GDP_PDU_IN --- read a packet from the network
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
#define GET48(v) \
		{ \
				v  = ((uint64_t) *pbp++) << 40; \
				v |= ((uint64_t) *pbp++) << 32; \
				v |= ((uint64_t) *pbp++) << 24; \
				v |= ((uint64_t) *pbp++) << 16; \
				v |= ((uint64_t) *pbp++) << 8; \
				v |= ((uint64_t) *pbp++); \
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


// read the fixed header portion in; shared with routing code
EP_STAT
_gdp_pdu_hdr_in(gdp_pdu_t *pdu,
		gdp_chan_t *chan,
		size_t *pduszp,
		uint64_t *dlenp)
{
	uint64_t dlen;
	uint8_t pbuf[_GDP_PDU_MAXHDRSZ];
	uint8_t *pbp;
	gdp_buf_t *ibuf;
	size_t needed;

	ibuf = bufferevent_get_input(chan->bev);

	// see if the fixed part of the header is all in
	needed = gdp_buf_peek(ibuf, pbuf, _GDP_PDU_FIXEDHDRSZ);

	if (ep_dbg_test(Dbg, 50))
	{
		ep_dbg_printf("_gdp_pdu_in: fixed pdu header:\n");
		ep_hexdump(pbuf, needed, ep_dbg_getfile(), EP_HEXDUMP_HEX, 0);
	}

	if (needed < _GDP_PDU_FIXEDHDRSZ)
	{
		// try again after we read more in
		ep_dbg_cprintf(Dbg, 42,
						"_gdp_pdu_in: keep reading (have %zd, need %d)\n",
						gdp_buf_getlength(ibuf), _GDP_PDU_FIXEDHDRSZ);
		return GDP_STAT_KEEP_READING;
	}

	// read the fixed part of the packet header in
	pbp = pbuf;
	pdu->ver = *pbp++;

	// no point in continuing if we don't recognize the PDU
	if (pdu->ver < GDP_PROTO_MIN_VERSION || pdu->ver > GDP_PROTO_CUR_VERSION)
	{
		ep_dbg_cprintf(Dbg, 1, "_gdp_pdu_in: version %d out of range (%d-%d)\n",
				pdu->ver, GDP_PROTO_MIN_VERSION, GDP_PROTO_CUR_VERSION);
		return GDP_STAT_PDU_VERSION_MISMATCH;
	}

	// ok, we recognize it
	pdu->ttl = *pbp++;
	pdu->rsvd1 = *pbp++;
	pdu->cmd = *pbp++;
	memcpy(pdu->dst, pbp, sizeof pdu->dst);
	pbp += sizeof pdu->dst;
	memcpy(pdu->src, pbp, sizeof pdu->src);
	pbp += sizeof pdu->src;
	pdu->sigalg = *pbp++;
	pdu->siglen = *pbp++ * 4;
	pdu->olen = *pbp++ * 4;
	pdu->flags = *pbp++;
	GET32(dlen);

	// do some error checking
	int olen = 0;
	if (EP_UT_BITSET(GDP_PDU_HAS_RID, pdu->flags))
		olen += sizeof (gdp_rid_t);
	if (EP_UT_BITSET(GDP_PDU_HAS_SEQNO, pdu->flags))
		olen += sizeof (gdp_seqno_t);
	if (EP_UT_BITSET(GDP_PDU_HAS_RECNO, pdu->flags))
		olen += sizeof (gdp_recno_t);
	if (EP_UT_BITSET(GDP_PDU_HAS_TS, pdu->flags))
		olen += sizeof (EP_TIME_SPEC);
	if (pdu->olen < olen)
	{
		// oops!  ten pounds in a five pound sack
		EP_STAT estat = GDP_STAT_PDU_CORRUPT;
		ep_log(estat,
				"_gdp_pdu_in: option overflow, needs %d, has %d",
				olen, pdu->olen);
		return estat;
	}

	// figure out how much additional data we will need
	needed += pdu->olen + dlen + pdu->siglen;
	*dlenp = dlen;
	*pduszp = needed;

	// see if the entire packet (header + data) is available
	if (gdp_buf_getlength(ibuf) < needed)
	{
		// still not enough data
		if (ep_dbg_test(Dbg, 42))
		{
			char xbuf[256];
			size_t s;

			s = gdp_buf_getlength(ibuf);
			ep_dbg_printf("_gdp_pdu_in: keep reading (have %zd, need %zd)\n",
						s, needed);
			if (s > sizeof xbuf)
				s = sizeof xbuf;
			gdp_buf_peek(ibuf, xbuf, s);
			ep_hexdump(xbuf, s, ep_dbg_getfile(), EP_HEXDUMP_HEX, 0);
		}
		return GDP_STAT_KEEP_READING;
	}

	return EP_STAT_OK;
}

EP_STAT
_gdp_pdu_in(gdp_pdu_t *pdu, gdp_chan_t *chan)
{
	EP_STAT estat = EP_STAT_OK;
	size_t sz;
	uint8_t pbuf[_GDP_PDU_MAXHDRSZ];
	uint8_t *pbp;
	gdp_buf_t *ibuf;
	size_t needed;
	uint64_t dlen;

	EP_ASSERT_POINTER_VALID(pdu);

	ep_dbg_cprintf(Dbg, 60, "_gdp_pdu_in\n");	// XXX
	EP_ASSERT(pdu->datum != NULL);
	EP_ASSERT(pdu->datum->dbuf != NULL);
	ibuf = bufferevent_get_input(chan->bev);

	estat = _gdp_pdu_hdr_in(pdu, chan, &needed, &dlen);
	EP_STAT_CHECK(estat, return estat);

	// the entire packet is now in ibuf

	// now drain the data we have processed
	sz = gdp_buf_read(ibuf, pbuf, _GDP_PDU_FIXEDHDRSZ + pdu->olen);
	if (sz < _GDP_PDU_FIXEDHDRSZ + pdu->olen)
	{
		// shouldn't happen, since it's already in memory
		estat = GDP_STAT_BUFFER_FAILURE;
		ep_log(estat,
				"_gdp_pdu_in: gdp_buf_drain failed, sz = %zu, needed = %u\n",
				sz, _GDP_PDU_FIXEDHDRSZ);
		// buffer is now out of sync; not clear if we can continue
	}
	pbp = &pbuf[_GDP_PDU_FIXEDHDRSZ];

	if (ep_dbg_test(Dbg, 32))
	{
		ep_dbg_printf("_gdp_pdu_in: read packet header:\n");
		ep_hexdump(pbuf, sz, ep_dbg_getfile(), EP_HEXDUMP_HEX, 0);
	}

	// Request Id
	if (!EP_UT_BITSET(GDP_PDU_HAS_RID, pdu->flags))
		pdu->rid = GDP_PDU_NO_RID;
	else
		GET32(pdu->rid);

	// record number
	if (!EP_UT_BITSET(GDP_PDU_HAS_RECNO, pdu->flags))
		pdu->datum->recno = GDP_PDU_NO_RECNO;
	else
		GET64(pdu->datum->recno)

	// sequence number
	if (!EP_UT_BITSET(GDP_PDU_HAS_SEQNO, pdu->flags))
		pdu->seqno = 0;
	else
		GET64(pdu->seqno)

	// timestamp
	if (!EP_UT_BITSET(GDP_PDU_HAS_TS, pdu->flags))
	{
		memset(&pdu->datum->ts, 0, sizeof pdu->datum->ts);
		EP_TIME_INVALIDATE(&pdu->datum->ts);
	}
	else
	{
		GET64(pdu->datum->ts.tv_sec);
		GET32(pdu->datum->ts.tv_nsec);
		GET32(*((uint32_t *) &pdu->datum->ts.tv_accuracy));
	}

	//XXX soak up any padding bytes?

	// ibuf now points at the data block
	{
		size_t l;

		ep_dbg_cprintf(Dbg, 38,
				"_gdp_pdu_in: reading %zd more bytes (%zd available)\n",
				dlen, evbuffer_get_length(ibuf));
		l = evbuffer_remove_buffer(ibuf, pdu->datum->dbuf, dlen);
		if (ep_dbg_test(Dbg, 39))
		{
			ep_hexdump(evbuffer_pullup(pdu->datum->dbuf, l), l,
					ep_dbg_getfile(), EP_HEXDUMP_ASCII,
					needed - dlen);
		}
		if (l < dlen)
		{
			// should never happen since we already have all the data in memory
			ep_dbg_cprintf(Dbg, 2,
					"_gdp_pdu_in: cannot read all data; wanted %zd, got %zd\n",
					dlen, l);
		}
	}

	if (ep_dbg_test(Dbg, 22))
	{
		char ebuf[200];

		ep_dbg_printf("_gdp_pdu_in => %s\n    ",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
		_gdp_pdu_dump(pdu, ep_dbg_getfile());
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

static EP_THR_MUTEX		PduFreeListMutex	EP_THR_MUTEX_INITIALIZER;
static TAILQ_HEAD(pkt_head, gdp_pdu)
						PduFreeList = TAILQ_HEAD_INITIALIZER (PduFreeList);

#ifdef GDP_PACKET_QUEUE
static EP_THR_MUTEX		ActivePduMutex		EP_THR_MUTEX_INITIALIZER;
static EP_THR_COND		ActivePduCond		EP_THR_COND_INITIALIZER;
static TAILQ_HEAD(active_head, gdp_pdu)
						ActivePacketQueue = TAILQ_HEAD_INITIALIZER(ActivePacketQueue);
#endif


/*
**  _GDP_PDU_NEW --- allocate a packet (from free list if possible)
**  _GDP_PDU_FREE --- return a packet to the free list
*/

gdp_pdu_t *
_gdp_pdu_new(void)
{
	gdp_pdu_t *pdu;

	ep_thr_mutex_lock(&PduFreeListMutex);
	if ((pdu = TAILQ_FIRST(&PduFreeList)) != NULL)
		TAILQ_REMOVE(&PduFreeList, pdu, list);
	ep_thr_mutex_unlock(&PduFreeListMutex);

	if (pdu == NULL)
	{
		pdu = ep_mem_zalloc(sizeof *pdu);
	}

	// initialize the packet
	EP_ASSERT(!pdu->inuse);
	memset(pdu, 0, sizeof *pdu);
	pdu->ver = GDP_PROTO_CUR_VERSION;
	pdu->ttl = GDP_TTL_DEFAULT;
	pdu->datum = gdp_datum_new();
	pdu->inuse = true;

	ep_dbg_cprintf(Dbg, 48, "_gdp_pdu_new => %p\n", pdu);
	return pdu;
}

void
_gdp_pdu_free(gdp_pdu_t *pdu)
{
	ep_dbg_cprintf(Dbg, 48, "_gdp_pdu_free(%p)\n", pdu);
	EP_ASSERT(pdu->inuse);
	if (pdu->datum != NULL)
		gdp_datum_free(pdu->datum);
	pdu->datum = NULL;
	pdu->inuse = false;
	ep_thr_mutex_lock(&PduFreeListMutex);
	TAILQ_INSERT_HEAD(&PduFreeList, pdu, list);
	ep_thr_mutex_unlock(&PduFreeListMutex);
}


#ifdef GDP_PACKET_QUEUE

/*
**  _GDP_PDU_ADD_TO_QUEUE --- add a packet to the active queue
**  _GDP_PDU_GET_ACTIVE --- get a packet from the active queue
*/

void
_gdp_pdu_add_to_queue(gdp_pdu_t *pdu)
{
	ep_thr_mutex_lock(&ActivePduMutex);
	TAILQ_INSERT_TAIL(&ActivePacketQueue, pdu, list);
	ep_thr_cond_signal(&ActivePduCond);
	ep_thr_mutex_unlock(&ActivePduMutex);
}

gdp_pdu_t *
_gdp_pdu_get_active(void)
{
	gdp_pdu_t *pdu;

	ep_thr_mutex_lock(&ActivePduMutex);
	while (TAILQ_EMPTY(&ActivePacketQueue))
	{
		ep_thr_cond_wait(&ActivePduCond, &ActivePduMutex, NULL);
	}
	pdu = TAILQ_FIRST(&ActivePacketQueue);
	TAILQ_REMOVE(&ActivePacketQueue, pdu, list);
	ep_thr_mutex_unlock(&ActivePduMutex);

	return pdu;
}
#endif
