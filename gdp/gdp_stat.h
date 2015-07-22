/* vim: set ai sw=4 sts=4 ts=4 : */

/***********************************************************************
**
**	GDP_STAT.H --- status codes specific to the Global Data Plane
**
**		If you add codes here, be sure to add a string description
**		in gdp_stat.c.
**
***********************************************************************/

#include <ep/ep_stat.h>

extern void		_gdp_stat_init(void);

// XXX	should really be in an include shared with other projects
//		to avoid conflicts in the future
#define GDP_MODULE			1

#define GDP_STAT_NEW(sev, det)		EP_STAT_NEW(EP_STAT_SEV_ ## sev,	\
										EP_REGISTRY_UCB, GDP_MODULE, det)

#define GDP_STAT_MSGFMT					GDP_STAT_NEW(ERROR, 1)
#define GDP_STAT_SHORTMSG				GDP_STAT_NEW(ERROR, 2)
#define GDP_STAT_READ_OVERFLOW			GDP_STAT_NEW(WARN, 3)
#define GDP_STAT_NOT_IMPLEMENTED		GDP_STAT_NEW(SEVERE, 4)
#define GDP_STAT_PDU_WRITE_FAIL			GDP_STAT_NEW(ERROR, 5)
#define GDP_STAT_PDU_READ_FAIL			GDP_STAT_NEW(ERROR, 6)
#define GDP_STAT_PDU_VERSION_MISMATCH	GDP_STAT_NEW(SEVERE, 7)
#define GDP_STAT_PDU_NO_SEQ				GDP_STAT_NEW(ERROR, 8)
#define GDP_STAT_KEEP_READING			GDP_STAT_NEW(WARN, 9)
#define GDP_STAT_NOT_OPEN				GDP_STAT_NEW(ERROR, 10)
#define GDP_STAT_UNKNOWN_RID			GDP_STAT_NEW(SEVERE, 11)
#define GDP_STAT_INTERNAL_ERROR			GDP_STAT_NEW(ABORT, 12)
#define GDP_STAT_BAD_IOMODE				GDP_STAT_NEW(ERROR, 13)
#define GDP_STAT_GCL_NAME_INVALID		GDP_STAT_NEW(ERROR, 14)
#define GDP_STAT_BUFFER_FAILURE			GDP_STAT_NEW(ABORT, 15)
#define GDP_STAT_NULL_GCL				GDP_STAT_NEW(ERROR, 16)
#define GDP_STAT_PROTOCOL_FAIL			GDP_STAT_NEW(SEVERE, 17)
#define GDP_STAT_CORRUPT_INDEX			GDP_STAT_NEW(SEVERE, 18)
#define GDP_STAT_CORRUPT_GCL			GDP_STAT_NEW(SEVERE, 19)
#define GDP_STAT_DEAD_DAEMON			GDP_STAT_NEW(ABORT, 20)
#define GDP_STAT_GCL_VERSION_MISMATCH	GDP_STAT_NEW(SEVERE, 21)
#define GDP_STAT_READONLY				GDP_STAT_NEW(ERROR, 22)
#define GDP_STAT_NOTFOUND				GDP_STAT_NEW(ERROR, 23)
#define GDP_STAT_PDU_CORRUPT			GDP_STAT_NEW(ABORT, 24)
#define GDP_STAT_SKEY_REQUIRED			GDP_STAT_NEW(ERROR, 25)

// create EP_STAT from GDP protocol command codes for acks and naks
//		values from 200 up are reserved for this
#define GDP_STAT_FROM_ACK(c)			GDP_STAT_NEW(OK, (c) - GDP_ACK_MIN + 200)
#define GDP_STAT_FROM_C_NAK(c)			GDP_STAT_NEW(ERROR, (c) - GDP_NAK_C_MIN + 400)
#define GDP_STAT_FROM_S_NAK(c)			GDP_STAT_NEW(SEVERE, (c) - GDP_NAK_S_MIN + 500)
#define GDP_STAT_FROM_R_NAK(c)			GDP_STAT_NEW(ERROR, (c) - GDP_NAK_R_MIN + 600)

// CoAp status codes (times 100): mapping to HTTP is shown in comments
//		CoAp based on RFC7252, HTTP based on 1.1
//		All CoAp codes have a dot after the first digit, e.g., 404 => 4.04
//		CoAp codes resemble HTTP codes, but they are not identical!
//		N/E displays HTTP codes that have no equivalent in CoAP.
#define GDP_COAP_SUCCESS		200		// Success (200)
#define GDP_COAP_CREATED		201		// Created (201)
#define GDP_COAP_DELETED		202		// Deleted (~204 No Content)
										// (N/E: 202 Accepted)
#define GDP_COAP_VALID			203		// Valid (~304 Not Modified)
										// (N/E: 203 Non-Authoritative Info)
#define GDP_COAP_CHANGED		204		// Changed (~204 No Content, POST/PUT only)
#define GDP_COAP_CONTENT		205		// Content (~200, GET only)
										// (N/E: 205 Reset Content)
										// (N/E: 206 Partial Content)

#define GDP_COAP_BADREQ			400		// Bad Request (400)
#define GDP_COAP_UNAUTH			401		// Unauthorized (401)
#define GDP_COAP_BADOPT			402		// Bad Option (not in HTTP)
										// (no equiv: 402 Payment Required)
#define GDP_COAP_FORBIDDEN		403		// Forbidden (403)
#define GDP_COAP_NOTFOUND		404		// Not Found (404)
#define GDP_COAP_METHNOTALLOWED	405		// Method Not Allowed (405)
#define GDP_COAP_NOTACCEPTABLE	406		// Not Acceptable (~406)
										// (N/E: 407 Proxy Auth Required)
										// (N/E: 408 Request Timeout)
										// (N/E: 409 Conflict)
										// (N/E: 410 Gone)
										// (N/E: Length Required)
#define GDP_COAP_PRECONFAILED	412		// Precondition Failed (412)
#define GDP_COAP_TOOLARGE		413		// Request Entity Too Large (413)
										// (N/E: 414 Request-URI Too Long)
#define GDP_COAP_UNSUPMEDIA		415		// Unsupported Content-Format (~415)

#define GDP_COAP_INTERNAL		500		// Internal Server Error (500)
#define GDP_COAP_NOTIMPL		501		// Not Implemented (501)
#define GDP_COAP_BADGATEWAY		502		// Bad Gateway (502)
#define GDP_COAP_SVCUNAVAIL		503		// Service Unavailable (~503)
#define GDP_COAP_GWTIMEOUT		504		// Gateway Timeout (504)
#define GDP_COAP_PROXYNOTSUP	505		// Proxying Not Supported (N/E)
										// (N/E: 505 HTTP Version Not Supported)

#define GDP_STAT_ACK_SUCCESS		GDP_STAT_NEW(OK, GDP_COAP_SUCCESS)
#define GDP_STAT_ACK_CREATED		GDP_STAT_NEW(OK, GDP_COAP_CREATED)
#define GDP_STAT_ACK_DELETED		GDP_STAT_NEW(OK, GDP_COAP_DELETED)
#define GDP_STAT_ACK_VALID			GDP_STAT_NEW(OK, GDP_COAP_VALID)
#define GDP_STAT_ACK_CHANGED		GDP_STAT_NEW(OK, GDP_COAP_CHANGED)
#define GDP_STAT_ACK_CONTENT		GDP_STAT_NEW(OK, GDP_COAP_CONTENT)

#define GDP_STAT_NAK_BADREQ			GDP_STAT_NEW(ERROR, GDP_COAP_BADREQ)
#define GDP_STAT_NAK_UNAUTH			GDP_STAT_NEW(ERROR, GDP_COAP_UNAUTH)
#define GDP_STAT_NAK_BADOPT			GDP_STAT_NEW(ERROR, GDP_COAP_BADOPT)
#define GDP_STAT_NAK_FORBIDDEN		GDP_STAT_NEW(ERROR, GDP_COAP_FORBIDDEN)
#define GDP_STAT_NAK_NOTFOUND		GDP_STAT_NEW(ERROR, GDP_COAP_NOTFOUND)
#define GDP_STAT_NAK_METHNOTALLOWED	GDP_STAT_NEW(ERROR, GDP_COAP_METHNOTALLOWED)
#define GDP_STAT_NAK_NOTACCEPTABLE	GDP_STAT_NEW(ERROR, GDP_COAP_NOTACCEPTABLE)
#define GDP_STAT_NAK_CONFLICT		GDP_STAT_NEW(ERROR, 409)
#define GDP_STAT_NAK_PRECONFAILED	GDP_STAT_NEW(ERROR, GDP_COAP_PRECONFAILED)
#define GDP_STAT_NAK_TOOLARGE		GDP_STAT_NEW(ERROR, GDP_COAP_TOOLARGE)
#define GDP_STAT_NAK_UNSUPMEDIA		GDP_STAT_NEW(ERROR, GDP_COAP_UNSUPMEDIA)

#define GDP_STAT_NAK_INTERNAL		GDP_STAT_NEW(SEVERE, GDP_COAP_INTERNAL)
#define GDP_STAT_NAK_NOTIMPL		GDP_STAT_NEW(SEVERE, GDP_COAP_NOTIMPL)
#define GDP_STAT_NAK_BADGATEWAY		GDP_STAT_NEW(SEVERE, GDP_COAP_BADGATEWAY)
#define GDP_STAT_NAK_SVCUNAVAIL		GDP_STAT_NEW(SEVERE, GDP_COAP_SVCUNAVAIL)
#define GDP_STAT_NAK_GWTIMEOUT		GDP_STAT_NEW(SEVERE, GDP_COAP_GWTIMEOUT)
#define GDP_STAT_NAK_PROXYNOTSUP	GDP_STAT_NEW(SEVERE, GDP_COAP_PROXYNOTSUP)

#define GDP_STAT_NAK_NOROUTE		GDP_STAT_NEW(ERROR, 600)
