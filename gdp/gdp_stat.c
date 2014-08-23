/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep_stat.h>
#include "gdp.h"
#include "gdp_stat.h"
#include "gdp_priv.h"

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

	{ NAKSTAT(C_BADREQ),				"4.00 bad request",				},
	{ NAKSTAT(C_UNAUTH),				"4.01 unauthorized",			},
	{ NAKSTAT(C_BADOPT),				"4.02 bad option",				},
	{ NAKSTAT(C_FORBIDDEN),				"4.03 forbidden",				},
	{ NAKSTAT(C_NOTFOUND),				"4.04 not found",				},
	{ NAKSTAT(C_METHNOTALLOWED),		"4.05 method not allowed",		},
	{ NAKSTAT(C_NOTACCEPTABLE),			"4.06 not acceptable",			},
	{ NAKSTAT(C_PRECONFAILED),			"4.12 precondition failed",		},
	{ NAKSTAT(C_TOOLARGE),				"4.13 request entity too large", },
	{ NAKSTAT(C_UNSUPMEDIA),			"4.15 unsupported media type",	},
	{ NAKSTAT(S_INTERNAL),				"5.00 internal server error",	},
	{ NAKSTAT(S_NOTIMPL),				"5.01 not implemented",			},
	{ NAKSTAT(S_BADGATEWAY),			"5.02 bad gateway",				},
	{ NAKSTAT(S_SVCUNAVAIL),			"5.03 service unavailable",		},
	{ NAKSTAT(S_GWTIMEOUT),				"5.04 gateway timeout",			},
	{ NAKSTAT(S_PROXYNOTSUP),			"5.05 proxying not supported",	},

	// end of list sentinel
	{ EP_STAT_OK,						NULL							},
};

void
_gdp_stat_init(void)
{
	ep_stat_reg_strings(Stats);
}
