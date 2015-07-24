/* vim: set ai sw=4 sts=4 ts=4 : */

#include "ep.h"
#include "ep_crypto.h"

#include <openssl/pem.h>
#if EP_CRYPTO_INCLUDE_RSA
# include <openssl/rsa.h>
#endif
#if EP_CRYPTO_INCLUDE_DSA
# include <openssl/dsa.h>
#endif
#if EP_CRYPTO_INCLUDE_EC
# include <openssl/ec.h>
#endif
#if EP_CRYPTO_INCLUDE_DH
# include <openssl/dh.h>
#endif

#include <string.h>

EP_CRYPTO_KEY *
ep_crypto_key_create(unsigned int keytype, unsigned int keylen)
{
	EVP_PKEY *key;

	key = EVP_PKEY_new();
	if (key == NULL)
		return _ep_crypto_error("Cannot create new keypair");

	switch (keytype)
	{
#if EP_CRYPTO_INCLUDE_RSA
		case EP_CRYPTO_KEYTYPE_RSA:
			{
				int exponent = ep_adm_getintparam("libep.crypto.rsa.key.exponent",
						3);
				if (keylen < EP_CRYPTO_KEY_MINLEN_RSA)
					return _ep_crypto_error("insecure RSA key length %d; %d min",
							keylen, EP_CRYPTO_KEY_MINLEN_RSA);
				RSA *rsakey = RSA_generate_key(keylen, exponent, NULL, NULL);
				if (rsakey == NULL)
					return _ep_crypto_error("cannot generate RSA key");
				if (EVP_PKEY_assign_RSA(key, rsakey) != 1)
					return _ep_crypto_error("cannot save RSA key");
			}
			break;
#endif

#if EP_CRYPTO_INCLUDE_DSA
		case EP_CRYPTO_KEYTYPE_DSA:
			{
				DSA *dsakey = DSA_generate_key(keylen, XXX);
				if (dsakey == NULL)
					return _ep_crypto_error("cannot generate DSA key");
				if (EVP_PKEY_assign_DSA(key, dsakey) != 1)
					return _ep_crypto_error("cannot save DSA key");
			}
			break;
#endif

#if EP_CRYPTO_INCLUDE_DH
		case EP_CRYPTO_KEYTYPE_DH:
			{
				DH *dhkey = DH_generate_key(keylen, XXX);
				if (dhkey == NULL)
					return _ep_crypto_error("cannot generate DH key");
				if (EVP_PKEY_assign_DH(key, dhkey) != 1)
					return _ep_crypto_error("cannot save DH key");
			}
			break;
#endif

#if EP_CRYPTO_INCLUDE_EC
		case EP_CRYPTO_KEYTYPE_EC:
			{
				EC *eckey = EC_generate_key(keylen, XXX);
				if (eckey == NULL)
					return _ep_crypto_error("cannot generate EC key");
				if (EVP_PKEY_assign_EC(key, eckey) != 1)
					return _ep_crypto_error("cannot save EC key");
			}
			break;
#endif

		default:
			return _ep_crypto_error("unrecognized key type %d", keytype);
	}
	return key;
}


void
ep_crypto_key_free(EP_CRYPTO_KEY *key)
{
	EVP_PKEY_free(key);
}


struct keyforms
{
	const char		*str;			// string name of form
	int				keyform;		// internal name
};

static struct keyforms	KeyFormStrings[] =
{
	{ "pem",			EP_CRYPTO_KEYFORM_PEM,		},
	{ "der",			EP_CRYPTO_KEYFORM_DER,		},
	{ NULL,				0							}
};

int
ep_crypto_keyform_fromstring(const char *fmt)
{
	struct keyforms *kf;

	for (kf = KeyFormStrings; kf->str != NULL; kf++)
	{
		if (strcasecmp(kf->str, fmt) == 0)
			break;
	}
	return kf->keyform;
}


EP_CRYPTO_KEY *
ep_crypto_key_read_fp(
		FILE *fp,
		const char *filename,
		int keytype,
		int keyform,
		uint32_t flags)
{
	EVP_PKEY *key = NULL;

	if (fp == NULL)
		return _ep_crypto_error("file must be specified");
	if (keyform <= 0)
		return _ep_crypto_error("keyform must be specified");

	switch (keyform)
	{
		case EP_CRYPTO_KEYFORM_PEM:
			if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
				key = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
			else
				key = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
			break;

		case EP_CRYPTO_KEYFORM_DER:
#if EP_CRYPTO_INCLUDE_DER
			// problematic: must know key type before reading it
			if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
				key = d2i_PrivateKey_fp(fp, NULL);
			else
				key = d2i_PUBKEY(fp, NULL);
			break;
#else
			return _ep_crypto_error("DER format not (yet) supported");
#endif

		default:
			return _ep_crypto_error("unknown key format %d", keyform);
	}
	if (key == NULL)
		return _ep_crypto_error("cannot read %s key from %s",
				EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags) ? "secret" : "public",
				filename);
	return key;
}



EP_CRYPTO_KEY *
ep_crypto_key_read_file(
		const char *filename,
		int keytype,
		int keyform,
		uint32_t flags)
{
	FILE *fp;
	EP_CRYPTO_KEY *key;

	if (keyform <= 0)
	{
		const char *p = strrchr(filename, '.');

		if (p != NULL)
			keyform = ep_crypto_keyform_fromstring(p);
	}
	if (keyform <= 0)
		return _ep_crypto_error("keyform must be specified");

	fp = fopen(filename, "r");
	if (fp == NULL)
		return _ep_crypto_error("cannot open key file %s", filename);
	key = ep_crypto_key_read_fp(fp, filename, keytype, keyform, flags);
	fclose(fp);
	return key;
}
