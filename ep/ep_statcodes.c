/* vim: set ai sw=8 sts=8 ts=8 : */

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
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
**  ----- END LICENSE BLOCK -----
***********************************************************************/

#include <ep/ep.h>

static struct ep_stat_to_string	Stats[] =
{
    // generic status codes
    { _EP_STAT_INTERNAL(OK, EP_STAT_MOD_GENERIC, 0), "generic"		},

    // individual codes
    { EP_STAT_WARN,		"generic warning",			},
    { EP_STAT_ERROR,		"generic error",			},
    { EP_STAT_SEVERE,		"generic severe error",			},
    { EP_STAT_ABORT,		"generic abort",			},

    { EP_STAT_OUT_OF_MEMORY,	"out of memory",			},
    { EP_STAT_ARG_OUT_OF_RANGE,	"argument out of range",		},
    { EP_STAT_END_OF_FILE,	"end of file",				},
    { EP_STAT_TIME_BADFORMAT,	"bad time format",			},
    { EP_STAT_BUF_OVERFLOW,	"potential buffer overflow",		},

    // cryptographic status codes
    { _EP_STAT_INTERNAL(OK, EP_STAT_MOD_CRYPTO, 0), "crypto"		},

    { EP_STAT_CRYPTO_DIGEST,	"cryptographic digest failure",		},
    { EP_STAT_CRYPTO_SIGN,	"cryptographic signing failure",	},
    { EP_STAT_CRYPTO_VRFY,	"cryptographic verification failure",	},
    { EP_STAT_CRYPTO_BADSIG,	"cryptographic signature doesn't match",},
    { EP_STAT_CRYPTO_KEYTYPE,	"unknown cryptographic key type",	},
    { EP_STAT_CRYPTO_KEYFORM,	"unknown cryptographic key format",	},
    { EP_STAT_CRYPTO_CONVERT,	"cannot encode/decode cryptographic key", },
    { EP_STAT_CRYPTO_KEYCREATE,	"cannot create new cryptographic key",	},
    { EP_STAT_CRYPTO_KEYCOMPAT,	"incompatible cryptographic keys",	},
    { EP_STAT_CRYPTO_CIPHER,	"symmetric cipher failure",		},

    { EP_STAT_OK,		NULL,					}
};


void
_ep_stat_init(void)
{
    ep_stat_reg_strings(Stats);
}
