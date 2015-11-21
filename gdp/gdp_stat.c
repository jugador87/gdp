/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	Status codes to string mappings
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**	----- END LICENSE BLOCK -----
*/

#include "gdp.h"
#include "gdp_stat.h"

/* TODO:	Verify that these strings are accurate.  */

#define NAKSTAT(code)	GDP_STAT_FROM_NAK(GDP_NAK_ ## code)

static struct ep_stat_to_string Stats[] =
{
	// module name
	{ GDP_STAT_NEW(OK, 0),				"Swarm-GDP",					},

	// individual codes
	{ GDP_STAT_MSGFMT,					"message format error",			},
	{ GDP_STAT_SHORTMSG,				"incomplete message",			},
	{ GDP_STAT_READ_OVERFLOW,			"read overflow",				},
	{ GDP_STAT_NOT_IMPLEMENTED,			"not implemented",				},
	{ GDP_STAT_PDU_WRITE_FAIL,			"network pdu write failure",	},
	{ GDP_STAT_PDU_READ_FAIL,			"network pdu read failure",		},
	{ GDP_STAT_PDU_VERSION_MISMATCH,	"protocol version mismatch",	},
	{ GDP_STAT_PDU_NO_SEQ,				"no sequence number",			},
	{ GDP_STAT_KEEP_READING,			"more input needed",			},
	{ GDP_STAT_NOT_OPEN,				"GCL is not open",				},
	{ GDP_STAT_UNKNOWN_RID,				"request id unknown",			},
	{ GDP_STAT_INTERNAL_ERROR,			"GDP internal error",			},
	{ GDP_STAT_BAD_IOMODE,				"GDP bad I/O mode",				},
	{ GDP_STAT_GCL_NAME_INVALID,		"invalid GCL name",				},
	{ GDP_STAT_BUFFER_FAILURE,			"gdp_buf I/O failure",			},
	{ GDP_STAT_NULL_GCL,				"GCL name required",			},
	{ GDP_STAT_PROTOCOL_FAIL,			"GDP protocol failure",			},
	{ GDP_STAT_CORRUPT_INDEX,			"corrupt GCL index",			},
	{ GDP_STAT_CORRUPT_GCL,				"corrupt GCL data file",		},
	{ GDP_STAT_DEAD_DAEMON,				"lost connection to GDP",		},
	{ GDP_STAT_GCL_VERSION_MISMATCH,	"gcl version mismatch",			},
	{ GDP_STAT_READONLY,				"cannot update read-only object",	},
	{ GDP_STAT_NOTFOUND,				"cannot find requested object",	},
	{ GDP_STAT_PDU_CORRUPT,				"corrupt pdu",					},
	{ GDP_STAT_SKEY_REQUIRED,			"secret key required",				},
	{ GDP_STAT_GCL_READ_ERROR,			"GCL read error",					},
	{ GDP_STAT_RECNO_SEQ_ERROR,			"record out of sequence",			},
	{ GDP_STAT_CRYPTO_SIGFAIL,			"signature failure",				},

	{ GDP_STAT_NAK_BADREQ,				"4.00 bad request",				},
	{ GDP_STAT_NAK_UNAUTH,				"4.01 unauthorized",			},
	{ GDP_STAT_NAK_BADOPT,				"4.02 bad option",				},
	{ GDP_STAT_NAK_FORBIDDEN,			"4.03 forbidden",				},
	{ GDP_STAT_NAK_NOTFOUND,			"4.04 not found",				},
	{ GDP_STAT_NAK_METHNOTALLOWED,		"4.05 method not allowed",		},
	{ GDP_STAT_NAK_NOTACCEPTABLE,		"4.06 not acceptable",			},
	{ GDP_STAT_NAK_CONFLICT,			"4_09 conflict",				},
	{ GDP_STAT_NAK_PRECONFAILED,		"4.12 precondition failed",		},
	{ GDP_STAT_NAK_TOOLARGE,			"4.13 request entity too large", },
	{ GDP_STAT_NAK_UNSUPMEDIA,			"4.15 unsupported media type",	},

	{ GDP_STAT_NAK_INTERNAL,			"5.00 internal server error",	},
	{ GDP_STAT_NAK_NOTIMPL,				"5.01 not implemented",			},
	{ GDP_STAT_NAK_BADGATEWAY,			"5.02 bad gateway",				},
	{ GDP_STAT_NAK_SVCUNAVAIL,			"5.03 service unavailable",		},
	{ GDP_STAT_NAK_GWTIMEOUT,			"5.04 gateway timeout",			},
	{ GDP_STAT_NAK_PROXYNOTSUP,			"5.05 proxying not supported",	},

	{ GDP_STAT_NAK_NOROUTE,				"6.00 no route available",		},

	// end of list sentinel
	{ EP_STAT_OK,						NULL							},
};

void
_gdp_stat_init(void)
{
	ep_stat_reg_strings(Stats);
}
