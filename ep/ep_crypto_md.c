/* vim: set ai sw=8 sts=8 ts=8 : */

#include <ep.h>
#include <ep_crypto.h>
#include <ep_dbg.h>

#include <strings.h>

static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto.md", "message digests");


const EVP_MD *
_ep_crypto_md_getalg_byid(int md_alg_id)
{
	switch (md_alg_id)
	{
	  case EP_CRYPTO_MD_SHA1:
		return EVP_sha1();

	  case EP_CRYPTO_MD_SHA224:
		return EVP_sha224();

	  case EP_CRYPTO_MD_SHA256:
		return EVP_sha256();

	  case EP_CRYPTO_MD_SHA384:
		return EVP_sha384();

	  case EP_CRYPTO_MD_SHA512:
		return EVP_sha512();

	  default:
		return _ep_crypto_error("unknown digest algorithm %d", md_alg_id);
	}
}


EP_CRYPTO_MD *
ep_crypto_md_new(int md_alg_id)
{
	const EVP_MD *md_alg;
	EP_CRYPTO_MD *md = EVP_MD_CTX_create();
	int istat;

	md_alg = _ep_crypto_md_getalg_byid(md_alg_id);
	if (md_alg == NULL)
		return NULL;		// error already given
	md = EVP_MD_CTX_create();
	if (md == NULL)
		return _ep_crypto_error("cannot create new message digest");
	istat = EVP_DigestInit_ex(md, md_alg, NULL);
	if (istat != 1)
	{
		EVP_MD_CTX_destroy(md);
		return _ep_crypto_error("cannot initialize message digest");
	}
	return md;
}


EP_CRYPTO_MD *
ep_crypto_md_clone(EP_CRYPTO_MD *oldmd)
{
	EP_CRYPTO_MD *md;
	int istat;

	md = EVP_MD_CTX_create();
	if (md == NULL)
		return _ep_crypto_error("cannot create cloned message digest");
	istat = EVP_MD_CTX_copy_ex(md, oldmd);
	if (istat != 1)
		return _ep_crypto_error("cannot clone message digest");

	return md;
}


EP_STAT
ep_crypto_md_update(EP_CRYPTO_MD *md, void *data, size_t dsize)
{
	int istat;

	istat = EVP_DigestUpdate(md, data, dsize);
	if (istat != 1)
	{
		(void) _ep_crypto_error("cannot update digest");
		return EP_STAT_CRYPTO_DIGEST;
	}
	return EP_STAT_OK;
}


EP_STAT
ep_crypto_md_final(EP_CRYPTO_MD *md, void *dbuf, size_t *dbufsize)
{
	int istat;
	unsigned int dbsize = *dbufsize;

	istat = EVP_DigestFinal_ex(md, dbuf, &dbsize);
	if (istat != 1)
	{
		(void) _ep_crypto_error("cannot finalize digest");
		return EP_STAT_CRYPTO_DIGEST;
	}
	*dbufsize = dbsize;
	return EP_STAT_OK;
}


void
ep_crypto_md_free(EP_CRYPTO_MD *md)
{
	EVP_MD_CTX_destroy(md);
}


int
ep_crypto_md_type(EP_CRYPTO_MD *md)
{
	int mdtype = EP_CRYPTO_MD_NULL;

	switch (EVP_MD_CTX_type(md))
	{
	  case NID_sha1:
		mdtype = EP_CRYPTO_MD_SHA1;
		break;

	  case NID_sha224:
		mdtype = EP_CRYPTO_MD_SHA224;
		break;

	  case NID_sha256:
		mdtype = EP_CRYPTO_MD_SHA256;
		break;

	  case NID_sha384:
		mdtype = EP_CRYPTO_MD_SHA384;
		break;

	  case NID_sha512:
		mdtype = EP_CRYPTO_MD_SHA512;
		break;

	  default:
		ep_dbg_cprintf(Dbg, 1, "ep_crypto_md_type: unknown md type %d\n",
				EVP_MD_CTX_type(md));
		break;
	}

	return mdtype;
}


struct name_to_format
{
	const char	*str;		// string name of format/algorithm
	int		form;		// internal name
};

static struct name_to_format	MdAlgStrings[] =
{
	{ "sha1",		EP_CRYPTO_MD_SHA1,		},
	{ "sha224",		EP_CRYPTO_MD_SHA224,		},
	{ "sha256",		EP_CRYPTO_MD_SHA256,		},
	{ "sha384",		EP_CRYPTO_MD_SHA384,		},
	{ "sha512",		EP_CRYPTO_MD_SHA512,		},
	{ NULL,			-1				}
};

int
ep_crypto_md_alg_byname(const char *fmt)
{
	struct name_to_format *kf;

	for (kf = MdAlgStrings; kf->str != NULL; kf++)
	{
		if (strcasecmp(kf->str, fmt) == 0)
			break;
	}
	return kf->form;
}

const char *
ep_crypto_md_alg_name(int md_alg)
{
	struct name_to_format *kf;

	for (kf = MdAlgStrings; kf->str != NULL; kf++)
	{
		if (kf->form == md_alg)
			return kf->str;
	}
	return "unknown";
}
