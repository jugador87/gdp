/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep_crypto.h>
#include <ep/ep_dbg.h>

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
			".:~/.gdp/keys:/usr/local/etc/gdp/keys:/etc/gdp/keys");
	char pbuf[1024];
	char *p = NULL;
	char *dir;
	int keytype = EP_CRYPTO_KEYTYPE_RSA;
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
				fmt = "%s/%s%s";
			snprintf(fnbuf, sizeof fnbuf, fmt, dir, basename, ext);
		}

		ep_dbg_cprintf(Dbg, 40,
				"_gdp_crypto_skey_read: trying %s\n",
				fnbuf);
		fp = fopen(fnbuf, "r");
		if (fp == NULL)
			continue;

		key = ep_crypto_key_read_fp(fp, fnbuf, keytype, keyform,
				EP_CRYPTO_F_SECRET);
		ep_dbg_cprintf(Dbg, 40,
				"_gdp_crypto_skey_read: found file, key = %p\n",
				key);
		fclose(fp);
		if (key != NULL)
			return key;
	}
	return NULL;
}
