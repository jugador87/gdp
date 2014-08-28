/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep_stat.h>
#include "gdp.h"
#include "gdp_stat.h"

/*
**	Status codes to string mappings
**
**	TODO	Verify that these strings are accurate.
*/

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
	{ GDP_STAT_PKT_WRITE_FAIL,			"network packet write failure",	},
	{ GDP_STAT_PKT_READ_FAIL,			"network packet read failure",	},
	{ GDP_STAT_PKT_VERSION_MISMATCH,	"protocol version mismatch",	},
	{ GDP_STAT_PKT_NO_SEQ,				"no sequence number",			},
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

	{ GDP_STAT_NAK_BADREQ,				"4.00 bad request",				},
	{ GDP_STAT_NAK_UNAUTH,				"4.01 unauthorized",			},
	{ GDP_STAT_NAK_BADOPT,				"4.02 bad option",				},
	{ GDP_STAT_NAK_FORBIDDEN,			"4.03 forbidden",				},
	{ GDP_STAT_NAK_NOTFOUND,			"4.04 not found",				},
	{ GDP_STAT_NAK_METHNOTALLOWED,		"4.05 method not allowed",		},
	{ GDP_STAT_NAK_NOTACCEPTABLE,		"4.06 not acceptable",			},
	{ GDP_STAT_NAK_PRECONFAILED,		"4.12 precondition failed",		},
	{ GDP_STAT_NAK_TOOLARGE,			"4.13 request entity too large", },
	{ GDP_STAT_NAK_UNSUPMEDIA,			"4.15 unsupported media type",	},
	{ GDP_STAT_NAK_INTERNAL,			"5.00 internal server error",	},
	{ GDP_STAT_NAK_NOTIMPL,				"5.01 not implemented",			},
	{ GDP_STAT_NAK_BADGATEWAY,			"5.02 bad gateway",				},
	{ GDP_STAT_NAK_SVCUNAVAIL,			"5.03 service unavailable",		},
	{ GDP_STAT_NAK_GWTIMEOUT,			"5.04 gateway timeout",			},
	{ GDP_STAT_NAK_PROXYNOTSUP,			"5.05 proxying not supported",	},

	// end of list sentinel
	{ EP_STAT_OK,						NULL							},
};

void
_gdp_stat_init(void)
{
	ep_stat_reg_strings(Stats);
}
