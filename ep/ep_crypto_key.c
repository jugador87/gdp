/* vim: set ai sw=8 sts=8 ts=8 : */

#include "ep.h"
#include "ep_crypto.h"

#include <openssl/pem.h>
#if _EP_CRYPTO_INCLUDE_RSA
# include <openssl/rsa.h>
#endif
#if _EP_CRYPTO_INCLUDE_DSA
# include <openssl/dsa.h>
#endif
#if _EP_CRYPTO_INCLUDE_EC
# include <openssl/ec.h>
#endif
#if _EP_CRYPTO_INCLUDE_DH
# include <openssl/dh.h>
#endif

#include <string.h>

// unclear in what version BIO_s_secmem appears....
#define BIO_s_secmem	BIO_s_mem


/*
**  Create new key pair
**
**	The keyexp parameter is the exponent parameter for RSA.
**	The curve parameter is for EC.
**	Both of these can be used by other types for other reasons.
**	If these are 0/NULL then a default is used.
*/

EP_CRYPTO_KEY *
ep_crypto_key_create(int keytype, int keylen, int keyexp, const char *curve)
{
	EVP_PKEY *key;

	key = EVP_PKEY_new();
	if (key == NULL)
		return _ep_crypto_error("Cannot create new keypair");

	switch (keytype)
	{
#if _EP_CRYPTO_INCLUDE_RSA
	  case EP_CRYPTO_KEYTYPE_RSA:
		if (keyexp <= 0)
			keyexp = ep_adm_getintparam("libep.crypto.rsa.key.exponent",
						3);
		if (keylen < EP_CRYPTO_KEY_MINLEN_RSA)
			return _ep_crypto_error("insecure RSA key length %d; %d min",
					keylen, EP_CRYPTO_KEY_MINLEN_RSA);
		RSA *rsakey = RSA_generate_key(keylen, keyexp, NULL, NULL);
		if (rsakey == NULL)
			return _ep_crypto_error("cannot generate RSA key");
		if (EVP_PKEY_assign_RSA(key, rsakey) != 1)
			return _ep_crypto_error("cannot save RSA key");
		break;
#endif

#if _EP_CRYPTO_INCLUDE_DSA
	  case EP_CRYPTO_KEYTYPE_DSA:
		DSA *dsakey = DSA_generate_key(keylen, XXX);
		if (dsakey == NULL)
			return _ep_crypto_error("cannot generate DSA key");
		if (EVP_PKEY_assign_DSA(key, dsakey) != 1)
			return _ep_crypto_error("cannot save DSA key");
		break;
#endif

#if _EP_CRYPTO_INCLUDE_DH
	  case EP_CRYPTO_KEYTYPE_DH:
		DH *dhkey = DH_generate_key(keylen, XXX);
		if (dhkey == NULL)
			return _ep_crypto_error("cannot generate DH key");
		if (EVP_PKEY_assign_DH(key, dhkey) != 1)
			return _ep_crypto_error("cannot save DH key");
		break;
#endif

#if _EP_CRYPTO_INCLUDE_EC
	  case EP_CRYPTO_KEYTYPE_EC:
		EC *eckey = EC_generate_key(keylen, XXX);
		if (eckey == NULL)
			return _ep_crypto_error("cannot generate EC key");
		if (EVP_PKEY_assign_EC(key, eckey) != 1)
			return _ep_crypto_error("cannot save EC key");
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


struct name_to_format
{
	const char	*str;		// string name of format/algorithm
	int		form;		// internal name
};

static struct name_to_format	KeyFormStrings[] =
{
	{ "pem",		EP_CRYPTO_KEYFORM_PEM,		},
	{ "der",		EP_CRYPTO_KEYFORM_DER,		},
	{ NULL,			0				}
};

int
ep_crypto_keyform_fromstring(const char *fmt)
{
	struct name_to_format *kf;

	for (kf = KeyFormStrings; kf->str != NULL; kf++)
	{
		if (strcasecmp(kf->str, fmt) == 0)
			break;
	}
	return kf->form;
}


static EP_CRYPTO_KEY *
key_read_bio(BIO *bio,
		const char *filename,
		int keytype,
		int keyform,
		uint32_t flags)
{
	EVP_PKEY *key = NULL;
	const char *pubsec = EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags) ?
		"secret" : "public";

	EP_ASSERT(bio != NULL);
	if (keyform <= 0)
		return _ep_crypto_error("keyform must be specified");


	if (keyform == EP_CRYPTO_KEYFORM_PEM)
	{
		// easy case
		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
		else
			key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
		if (key == NULL)
			return _ep_crypto_error("cannot read %s key", pubsec);
		goto finis;
	}
	else if (keyform != EP_CRYPTO_KEYFORM_DER)
	{
		return _ep_crypto_error("unknown key format %d", keyform);
	}

#if _EP_CRYPTO_INCLUDE_DER
	// for DER we have to do separate decoding based on key type
	key = EVP_PKEY_new();
	switch (keytype)
	{
# if _EP_CRYPTO_INCLUDE_RSA
	  case EP_CRYPTO_KEYTYPE_RSA:
		;					// compiler glitch
		RSA *rsakey;

		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			rsakey = d2i_RSAPrivateKey_bio(bio, NULL);
		else
			rsakey = d2i_RSA_PUBKEY_bio(bio, NULL);
		if (rsakey == NULL)
			return _ep_crypto_error("cannot decode RSA %s DER key",
					pubsec);
		if (EVP_PKEY_assign_RSA(key, rsakey) != 1)
			return _ep_crypto_error("cannot assign RSA %s DER key",
					pubsec);
		break;
# endif

# if _EP_CRYPTO_INCLUDE_DSA
	  case EP_CRYPTO_KEYTYPE_DSA:
		;					// compiler glitch
		DSA *dsakey;

		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			dsakey = d2i_DSAPrivateKey_bio(bio, NULL);
		else
			dsakey = d2i_DSA_PUBKEY_bio(bio, NULL);
		if (dsakey == NULL)
			return _ep_crypto_error("cannot decode DSA %s DER key",
					pubsec);
		if (EVP_PKEY_assign_DSA(key, dsakey) != 1)
			return _ep_crypto_error("cannot assign DSA %s DER key",
					pubsec);
		break;
# endif

# if _EP_CRYPTO_INCLUDE_EC
	  case EP_CRYPTO_KEYTYPE_EC:
		;					// compiler glitch
		EC_KEY *eckey;

		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			eckey = d2i_ECPrivateKey_bio(bio, NULL);
		else
			eckey = d2i_EC_PUBKEY_bio(bio, NULL);
		if (eckey == NULL)
			return _ep_crypto_error("cannot decode EC %s DER key",
					pubsec);
		if (EVP_PKEY_assign_EC_KEY(key, eckey) != 1)
			return _ep_crypto_error("cannot save EC %s DER key",
					pubsec);
		break;
# endif

# if _EP_CRYPTO_INCLUDE_DH
	  case EP_CRYPTO_KEYTYPE_DH:
		;					// compiler glitch
		DH *dhkey;

		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			dhkey = d2i_DHPrivateKey_bio(bio, NULL);
		else
			dhkey = d2i_DH_PUBKEY_bio(bio, NULL);
		if (dhkey == NULL)
			return _ep_crypto_error("cannot decode DH %s DER key",
					pubsec);
		if (EVP_PKEY_assign_DH_KEY(key, dhkey) != 1)
			return _ep_crypto_error("cannot save DH %s DER key",
					pubsec);
		break;
# endif

	  default:
		return _ep_crypto_error("unknown key type %d", keytype);
	}
#else
	return _ep_crypto_error("DER format not (yet) supported");
#endif // _EP_CRYPTO_INCLUDE_DER


finis:
	if (key == NULL)
		return _ep_crypto_error("cannot read %s key from %s",
				EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags) ? "secret" : "public",
				filename);
	return key;
}


EP_CRYPTO_KEY *
ep_crypto_key_read_fp(
		FILE *fp,
		const char *filename,
		int keytype,
		int keyform,
		uint32_t flags)
{
	EP_CRYPTO_KEY *key;
	BIO *bio = BIO_new_fp(fp, BIO_NOCLOSE);

	key = key_read_bio(bio, filename, keytype, keyform, flags);
	BIO_free(bio);
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

	// determine key format from file name (heuristic)
	if (keyform <= 0)
	{
		const char *p = strrchr(filename, '.');

		if (p != NULL)
			keyform = ep_crypto_keyform_fromstring(++p);
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


EP_CRYPTO_KEY *
ep_crypto_key_read_mem(
		void *buf,
		size_t buflen,
		int keytype,
		int keyform,
		uint32_t flags)
{
	EP_CRYPTO_KEY *key;
	BIO *bio = BIO_new_mem_buf(buf, buflen);

	key = key_read_bio(bio, "memory", keytype, keyform, flags);
	BIO_free(bio);
	return key;
}


/*
**  Write a key to a BIO output (internal use)
**
**	Have to use BIO because it's the only way to find out how
**	long the output is (that I could find).
**
**	XXX	Doesn't have any way to specify encryption when writing
**	XXX	private keys.
**	For PEM the calling convention is:
**	    PEM_write_*(fp, key, cipher, kstr, klen, pwcb, u)
**	where cipher is the key encryption cipher, kstr & klen point
**	at the password, pwcb points to a routine to use to read a
**	password, and u is a parameter to pwcb.
*/

static EP_STAT
key_write_bio(EP_CRYPTO_KEY *key,
		BIO *bio,
		int keyform,
		int cipher,
		uint32_t flags)
{
	int keytype = ep_crypto_key_id_fromkey(key);
	const char *pubsec = EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags) ?
		"secret" : "public";
	int istat;

	EP_ASSERT(bio != NULL);

	if (keyform <= 0)
	{
		(void) _ep_crypto_error("keyform must be specified");
		return EP_STAT_CRYPTO_CONVERT;
	}

	if (keyform == EP_CRYPTO_KEYFORM_PEM)
	{
		// easy case
		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			istat = PEM_write_bio_PrivateKey(bio, key, NULL,
					NULL, 0, NULL, NULL);
		else
			istat = PEM_write_bio_PUBKEY(bio, key);
		if (istat != 1)
		{
			(void) _ep_crypto_error("cannot write %s PEM key",
					pubsec);
			return EP_STAT_CRYPTO_CONVERT;
		}
		goto finis;
	}
	else if (keyform != EP_CRYPTO_KEYFORM_DER)
	{
		(void) _ep_crypto_error("unknown key format %d", keyform);
		return EP_STAT_CRYPTO_KEYFORM;
	}

#if _EP_CRYPTO_INCLUDE_DER
	const char *type;
	switch (keytype)
	{
# if _EP_CRYPTO_INCLUDE_RSA
	  case EP_CRYPTO_KEYTYPE_RSA:
		;					// compiler glitch
		RSA *rsakey = EVP_PKEY_get1_RSA(key);

		type = "RSA";
		if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
			istat = i2d_RSAPrivateKey_bio(bio, rsakey);
		else
			istat = i2d_RSA_PUBKEY_bio(bio, rsakey);
		if (istat != 1)
			goto fail1;


		if (i2d_RSAPublicKey_bio(bio, rsakey) < 0)
		{
			(void) _ep_crypto_error("cannot convert RSA pub key to DER");
			return EP_STAT_CRYPTO_KEYTYPE;
		}
		break;
# endif

# if _EP_CRYPTO_INCLUDE_DSA
	  case EP_CRYPTO_KEYTYPE_DSA:
		;					// compiler glitch
		DSA *dsakey = EVP_PKEY_get1_DSA(key);
		if (i2d_DSAPublicKey(bio, dsakey) < 0)
		{
			(void) _ep_crypto_error("cannot convert DSA pub key to DER");
			return EP_STAT_CRYPTO_KEYTYPE;
		}
		break;
# endif

# if _EP_CRYPTO_INCLUDE_ECDSA
	  case EP_CRYPTO_KEYTYPE_ECDSA:
		;					// compiler glitch
		EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(key);
		do what is needed
		break;
# endif

# if _EP_CRYPTO_INCLUDE_DH
	  case EP_CRYPTO_KEYTYPE_DH:
		;					// compiler glitch
		DH *dhkey = EVP_PKEY_get1_DH(key);
		do what is needed
		break;
# endif

	  default:
		_ep_crypto_error("unknown key type %d", keytype);
		return EP_STAT_CRYPTO_KEYTYPE;
	}
#endif // _EP_CRYPTO_INCLUDE_DER

finis:
	return EP_STAT_FROM_INT(BIO_ctrl_pending(bio));

fail1:
	(void) _ep_crypto_error("cannot encode %s %s DER key", type, pubsec);
	return EP_STAT_CRYPTO_CONVERT;
}


EP_STAT
ep_crypto_key_write_fp(EP_CRYPTO_KEY *key,
		FILE *fp,
		int keyform,
		int cipher,
		uint32_t flags)
{
	EP_STAT estat;
	BIO *bio;

	if (fp == NULL)
	{
		(void) _ep_crypto_error("file must be specified");
		return EP_STAT_CRYPTO_CONVERT;
	}

	bio = BIO_new_fp(fp, BIO_NOCLOSE);
	estat = key_write_bio(key, bio, keyform, cipher, flags);
	BIO_free(bio);
	return estat;
}


EP_STAT
ep_crypto_key_write_mem(EP_CRYPTO_KEY *key,
		void *buf,
		size_t buflen,
		int keyform,
		int cipher,
		uint32_t flags)
{
	EP_STAT estat;
	BIO *bio;

	if (EP_UT_BITSET(EP_CRYPTO_F_SECRET, flags))
		bio = BIO_new(BIO_s_secmem());
	else
		bio = BIO_new(BIO_s_mem());
	estat = key_write_bio(key, bio, keyform, cipher, flags);
	if (EP_STAT_ISOK(estat))
	{
		// save the memory
		size_t len = EP_STAT_TO_INT(estat);

		if (len > buflen)
		{
			(void) _ep_crypto_error("external keyform too long, wants %z, needs %z",
					len, buflen);
			estat = EP_STAT_CRYPTO_CONVERT;
		}
		else
		{
			void *p;

			BIO_get_mem_data(bio, &p);
			memcpy(buf, p, len);
		}
	}

	BIO_free(bio);
	return estat;
}


int
ep_crypto_key_id_fromkey(EP_CRYPTO_KEY *key)
{
	int i = EVP_PKEY_type(key->type);

	switch (i)
	{
	  case EVP_PKEY_RSA:
		return EP_CRYPTO_KEYTYPE_RSA;

	  case EVP_PKEY_DSA:
		return EP_CRYPTO_KEYTYPE_DSA;

	  case EVP_PKEY_DH:
		return EP_CRYPTO_KEYTYPE_DH;

	  case EVP_PKEY_EC:
		return EP_CRYPTO_KEYTYPE_EC;

	  default:
		return EP_CRYPTO_KEYTYPE_UNKNOWN;
	}
}

static struct name_to_format	KeyTypeStrings[] =
{
	{ "rsa",		EP_CRYPTO_KEYTYPE_RSA,		},
	{ "dsa",		EP_CRYPTO_KEYTYPE_DSA,		},
	{ "ec",			EP_CRYPTO_KEYTYPE_EC,		},
	{ "dh",			EP_CRYPTO_KEYTYPE_DH,		},
	{ NULL,			0				}
};

int
ep_crypto_key_id_byname(const char *fmt)
{
	struct name_to_format *kt;

	for (kt = KeyTypeStrings; kt->str != NULL; kt++)
	{
		if (strcasecmp(kt->str, fmt) == 0)
			break;
	}
	return kt->form;
}
