/* vim: set ai sw=4 sts=4 ts=4 : */

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
#include <ep_crypto.h>
#include <ep_dbg.h>

#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>


//static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto", "crypto support");

/*
**  LIBEP CRYPTOGRAPHIC SUPPORT
*/


/*
**  Initialize crypto support.
**		This basically loads everything.  If running in a small
**		address space you might want to adjust this.
*/

void
ep_crypto_init(uint32_t flags)
{
	static bool initialized = false;

	if (initialized)
		return;
	OPENSSL_load_builtin_modules();
	OpenSSL_add_all_algorithms();

	if (ep_adm_getboolparam("libep.crypto.dev", true))
		ENGINE_load_cryptodev();
	initialized = true;
}


/*
**  Internal helper for printing errors.
*/

void *
_ep_crypto_error(const char *msg, ...)
{
	va_list ap;
	FILE *fp = ep_dbg_getfile();
	static bool initialized = false;

	// load openssl error strings if not already done
	if (!initialized)
	{
		ERR_load_crypto_strings();
		initialized = true;
	}

	//XXX should be on some flag (don't print unconditionally)
	va_start(ap, msg);
	ep_dbg_printf("EP Crypto Error: ");
	vfprintf(fp, msg, ap);
	ep_dbg_printf("\n");
	ERR_print_errors_fp(fp);

	return NULL;
}
