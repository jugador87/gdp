/* vim: set ai sw=8 sts=8 ts=8 : */

#include <ep/ep.h>

static struct ep_stat_to_string	Stats[] =
{
    // generic status codes
    { _EP_STAT_INTERNAL(OK, EP_STAT_MOD_GENERIC, 0), "EPlib:generic"	},

    // individual codes
    { EP_STAT_WARN,		"generic warning",		},
    { EP_STAT_ERROR,		"generic error",		},
    { EP_STAT_SEVERE,		"generic severe error",		},
    { EP_STAT_ABORT,		"generic abort",		},

    { EP_STAT_OUT_OF_MEMORY,	"out of memory",		},
    { EP_STAT_ARG_OUT_OF_RANGE,	"argument out of range",	},
    { EP_STAT_END_OF_FILE,	"end of file",			},
    { EP_STAT_TIME_BADFORMAT,	"bad time format",		},

    // cryptographic status codes
    { _EP_STAT_INTERNAL(OK, EP_STAT_MOD_GENERIC, 0), "EPlib:crypto"	},

    { EP_STAT_CRYPTO_DIGEST,	"cryptographic digest failure",		},
    { EP_STAT_CRYPTO_SIGN,	"cryptographic signing failure",	},
    { EP_STAT_CRYPTO_VRFY,	"cryptographic verification failure",	},
    { EP_STAT_CRYPTO_BADSIG,	"cryptographic signature doesn't match",},
    { EP_STAT_CRYPTO_KEYTYPE,	"unknown cryptographic key type",	},
    { EP_STAT_CRYPTO_KEYFORM,	"unknown cryptographic key format",	},
    { EP_STAT_CRYPTO_CONVERT,	"cannot encode/decode cryptographic key", },
    { EP_STAT_CRYPTO_KEYCREATE,	"cannot create new cryptographic key",	},
    { EP_STAT_CRYPTO_KEYCOMPAT,	"incompatible cryptographic keys",	},

    { EP_STAT_OK,		NULL,				}
};


void
_ep_stat_init(void)
{
    ep_stat_reg_strings(Stats);
}
