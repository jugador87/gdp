/* vim: set ai sw=8 sts=8 ts=8 : */

#include <ep/ep_stat.h>

static struct ep_stat_to_string	Stats[] =
{

    // module name
    { _EP_STAT_INTERNAL(OK, EP_STAT_MOD_GENERIC, 0), "EPlib-generic" },

    // individual codes
    { EP_STAT_WARN,		"generic warning",		},
    { EP_STAT_ERROR,		"generic error",		},
    { EP_STAT_SEVERE,		"generic severe error",		},
    { EP_STAT_ABORT,		"generic abort",		},

    { EP_STAT_OUT_OF_MEMORY,	"out of memory",		},
    { EP_STAT_ARG_OUT_OF_RANGE,	"argument out of range",	},
    { EP_STAT_END_OF_FILE,	"end of file",			},

    { EP_STAT_OK,		NULL,				}
};


void
_ep_stat_init(void)
{
    ep_stat_reg_strings(Stats);
}
