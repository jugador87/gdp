/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  ----- BEGIN LICENSE BLOCK -----
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
**  ----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_crypto.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>

#include "gdp.h"

#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.crypto", "cryptographic operations for GDP");

/*
**  Read a secret key
**
**  This should have a better way of handling password-protected keys.
**  At the moment it uses the OpenSSL built-in "read from keyboard"
**  method.
*/

EP_CRYPTO_KEY *
_gdp_crypto_skey_read(const char *basename, const char *ext)
{
	const char *path = ep_adm_getstrparam("swarm.gdp.crypto.key.path",
			".:"
			"KEYS:"
			"~/.swarm/gdp/keys:"
			"/usr/local/etc/swarm/gdp/keys:"
			"/etc/swarm/gdp/keys");
	char pbuf[1024];
	char *p = NULL;
	char *dir;
	int keyform = EP_CRYPTO_KEYFORM_PEM;
	int i;

	if ((i = strlen(basename)) > 4)
	{
		if (strcmp(&basename[i - 4], ".pem") == 0)
		{
			ext = NULL;
			keyform = EP_CRYPTO_KEYFORM_PEM;
		}
		else if (strcmp(&basename[i - 4], ".der") == 0)
		{
			ext = NULL;
			keyform = EP_CRYPTO_KEYFORM_DER;
		}
	}

	strlcpy(pbuf, path, sizeof pbuf);
	for (dir = pbuf; dir != NULL; dir = p)
	{
		FILE *fp;
		EP_CRYPTO_KEY *key;
		char fnbuf[512];
		const char *fmt;

		p = strchr(dir, ':');
		if (p != NULL)
			*p++ = '\0';
		if (*dir == '\0')
			continue;

		// create the candidate filename
		if (strncmp(dir, "~/", 2) == 0)
		{
			char *home = getenv("HOME");

			if (home == NULL)
				continue;
			if (ext == NULL)
				fmt = "%s%s/%s";
			else
				fmt = "%s%s/%s.%s";
			snprintf(fnbuf, sizeof fnbuf, fmt, home, dir + 1, basename, ext);
		}
		else
		{
			if (ext == NULL)
				fmt = "%s/%s";
			else
				fmt = "%s/%s.%s";
			snprintf(fnbuf, sizeof fnbuf, fmt, dir, basename, ext);
		}

		ep_dbg_cprintf(Dbg, 40,
				"_gdp_crypto_skey_read: trying %s\n",
				fnbuf);
		fp = fopen(fnbuf, "r");
		if (fp == NULL)
			continue;

		key = ep_crypto_key_read_fp(fp, fnbuf, keyform, EP_CRYPTO_F_SECRET);
		ep_dbg_cprintf(Dbg, 40,
				"_gdp_crypto_skey_read: found file, key = %p\n",
				key);
		fclose(fp);
		if (key != NULL)
			return key;
	}
	return NULL;
}
