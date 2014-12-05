/* vim: set ai sw=4 sts=4 ts=4 : */

#ifndef _GDP_PKT_H_
#define _GDP_PKT_H_

#include <stdio.h>
#include <netinet/in.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include "gdp_priv.h"

#define GDP_PORT_DEFAULT		2468	// default IP port

#define GDP_PROTO_CUR_VERSION	1		// current protocol version
#define GDP_PROTO_MIN_VERSION	1		// min version we can accept



/*
**	Header for a GDP Protocol Data Unit.
**
**		This is not the "on the wire" format, which has to be put into
**		network byte order and packed.	However, this does show the
**		order in which fields are written.
**
**		Commands are eight bits with the top three bits encoding
**		additional semantics.  Those bits are:
**
**		00x		"Blind" (unacknowledged) command
**		01x		Acknowledged command
**		10x		Positive acknowledgement
**		110		Negative acknowledgement, client side problem
**		111		Negative acknowledgement, server side problem
**
**		These roughly correspond to the "Type" and "Code" class
**		field in the CoAP header.
**
**		XXX We may still want to play with these allocations,
**			depending on how dense the various spaces become.  I
**			suspect that "acknowledged command" will have the
**			most values and the ack/naks won't have very many.
**			Remember in particular that the commands have to include
**			the commands between gdpds for things like migration,
**			resource negotiation, etc.
**
**		XXX CoAP has two "sequence numbers": a message-id which
**			relates ack/naks to commands and a "token" which is
**			a higher level construct relating (for example)
**			subscribe requests to results.	The "rid" represents
**			a shorter version of the "token".  We don't include
**			"seq" since this is a lower-level concept that is
**			subsumed by TCP.
**
**		The structure of an on-the-wire packet is:
**			1	protocol version
**			1	command or ack/nak
**			1	flags (indicate presence/lack of additional fields)
**			1	reserved (must be zero on send, ignored on receive
**			4	length of data portion
**			[4	request id (optional)]
**			[32	GCL name (optional)]
**			[8	record number (optional)]
**			[16	commit timestamp (optional)]
**			N	data (length indicated above)
**		The structure shown below is the in-memory version
*/

typedef struct gdp_pkt
{
	// metadata
	TAILQ_ENTRY(gdp_pkt)	list;		// work list
//	gdp_chan_t				*chan;		// I/O channel for this packet entry
	bool					inuse:1;	// indicates that this is allocated

	// packet data
	uint8_t				ver;		// protocol version
	uint8_t				cmd;		// command or ack/nak
	uint8_t				flags;		// see below
	uint8_t				reserved1;	// must be zero on send, ignored on receive
	gdp_rid_t			rid;		// sequence number (GDP_PKT_NO_RID => none)
	gcl_name_t			gcl_name;	// name of the GCL of interest (0 => none)
	gdp_datum_t			*datum;		// pointer to data record
} gdp_pkt_t;

#define _GDP_MAX_PKT_HDR		128		// max size of on-wire packet header

/*
**  Protocol command values
**
**		The ACK and NAK values are tightly coupled with EP_STAT codes
**		and with COAP status codes, hence the somewhat baroque approach
**		here.
*/

#define _GDP_ACK_FROM_COAP(c)	(GDP_COAP_##c - 200 + GDP_ACK_MIN)
#define _GDP_NAK_C_FROM_COAP(c)	(GDP_COAP_##c - 400 + GDP_NAK_C_MIN)
#define _GDP_NAK_S_FROM_COAP(c)	(GDP_COAP_##c - 500 + GDP_NAK_S_MIN)

//		0-63			Blind commands
#define GDP_CMD_KEEPALIVE		0		// used for keepalives
//		64-127			Acknowledged commands
#define GDP_CMD_PING			64		// test connection
#define GDP_CMD_HELLO			65		// initial startup/handshake
#define GDP_CMD_CREATE			66		// create a GCL
#define GDP_CMD_OPEN_AO			67		// open a GCL for append-only
#define GDP_CMD_OPEN_RO			68		// open a GCL for read-only
#define GDP_CMD_CLOSE			69		// close a GCL
#define GDP_CMD_READ			70		// read a given record by index
#define GDP_CMD_PUBLISH			71		// append a record
#define GDP_CMD_SUBSCRIBE		72		// subscribe to a GCL
#define GDP_CMD_MULTIREAD		73		// read more than one records
#define GDP_CMD_GETMETADATA		74		// fetch metadata
//		128-191			Positive acks
#define GDP_ACK_MIN			128				// minimum ack code
#define GDP_ACK_SUCCESS			_GDP_ACK_FROM_COAP(SUCCESS)				// 128
#define GDP_ACK_CREATED			_GDP_ACK_FROM_COAP(CREATED)				// 129
#define GDP_ACK_DELETED			_GDP_ACK_FROM_COAP(DELETED)				// 130
#define GDP_ACK_VALID			_GDP_ACK_FROM_COAP(VALID)				// 131
#define GDP_ACK_CHANGED			_GDP_ACK_FROM_COAP(CHANGED)				// 132
#define GDP_ACK_CONTENT			_GDP_ACK_FROM_COAP(CONTENT)				// 133
#define GDP_ACK_MAX			191				// maximum ack code
//		192-223			Negative acks, client side (CoAP, HTTP)
#define GDP_NAK_C_MIN		192				// minimum client-side nak code
#define GDP_NAK_C_BADREQ		_GDP_NAK_C_FROM_COAP(BADREQ)			// 192
#define GDP_NAK_C_UNAUTH		_GDP_NAK_C_FROM_COAP(UNAUTH)			// 193
#define GDP_NAK_C_BADOPT		_GDP_NAK_C_FROM_COAP(BADOPT)			// 194
#define GDP_NAK_C_FORBIDDEN		_GDP_NAK_C_FROM_COAP(FORBIDDEN)			// 195
#define GDP_NAK_C_NOTFOUND		_GDP_NAK_C_FROM_COAP(NOTFOUND)			// 196
#define GDP_NAK_C_METHNOTALLOWED _GDP_NAK_C_FROM_COAP(METHNOTALLOWED)	// 197
#define GDP_NAK_C_NOTACCEPTABLE _GDP_NAK_C_FROM_COAP(NOTACCEPTABLE)		// 198
#define GDP_NAK_C_CONFLICT		(409 - 400 + GDP_NAK_C_MIN)				// 201
#define GDP_NAK_C_PRECONFAILED	_GDP_NAK_C_FROM_COAP(PRECONFAILED)		// 204
#define GDP_NAK_C_TOOLARGE		_GDP_NAK_C_FROM_COAP(TOOLARGE)			// 205
#define GDP_NAK_C_UNSUPMEDIA	_GDP_NAK_C_FROM_COAP(UNSUPMEDIA)		// 207
#define GDP_NAK_C_MAX		223				// maximum client-side nak code
//		224-254			Negative acks, server side (CoAP, HTTP)
#define GDP_NAK_S_MIN		224				// minimum server-side nak code
#define GDP_NAK_S_INTERNAL		_GDP_NAK_S_FROM_COAP(INTERNAL)			// 224
#define GDP_NAK_S_NOTIMPL		_GDP_NAK_S_FROM_COAP(NOTIMPL)			// 225
#define GDP_NAK_S_BADGATEWAY	_GDP_NAK_S_FROM_COAP(BADGATEWAY)		// 226
#define GDP_NAK_S_SVCUNAVAIL	_GDP_NAK_S_FROM_COAP(SVCUNAVAIL)		// 227
#define GDP_NAK_S_GWTIMEOUT		_GDP_NAK_S_FROM_COAP(GWTIMEOUT)			// 228
#define GDP_NAK_S_PROXYNOTSUP	_GDP_NAK_S_FROM_COAP(PROXYNOTSUP)		// 229
#define GDP_NAK_S_MAX		254				// maximum server-side nak code
//		255				Reserved


/***** values for gdp_pkg_hdr flags field *****/
#define GDP_PKT_HAS_RID		0x01		// has a rid field
#define GDP_PKT_HAS_ID		0x02		// has a gcl_name field
#define GDP_PKT_HAS_RECNO	0x04		// has a recno field
#define GDP_PKT_HAS_TS		0x08		// has a timestamp field

/***** dummy values for other fields *****/
#define GDP_PKT_NO_RID		0L			// no request id
#define GDP_PKT_NO_RECNO	-1			// no record number


gdp_pkt_t	*_gdp_pkt_new(void);		// allocate a new packet

void		_gdp_pkt_free(gdp_pkt_t *);	// free a packet

EP_STAT		_gdp_pkt_out(				// send a packet to a network buffer
				gdp_pkt_t *,			// the packet information
				gdp_chan_t *);			// the network channel

void		_gdp_pkt_out_hard(			// send a packet to a network buffer
				gdp_pkt_t *,			// the packet information
				gdp_chan_t *);			// the network channel

EP_STAT		_gdp_pkt_in(				// read a packet from a network buffer
				gdp_pkt_t *,			// the buffer to store the result
				gdp_chan_t *);			// the network channel

void		_gdp_pkt_dump(
				gdp_pkt_t *pkt,
				FILE *fp);

// generic sockaddr union	XXX does this belong in this header file?
union sockaddr_xx
{
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6 sin6;
};

#endif // _GDP_PKT_H_
