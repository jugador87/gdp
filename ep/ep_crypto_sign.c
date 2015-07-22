/* vim: set ai sw=8 sts=8 ts=8 : */

#include <ep/ep.h>
#include <ep_crypto.h>
#include <ep_dbg.h>

//static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto.sign", "cryptographic signatures");

EP_CRYPTO_MD *
ep_crypto_sign_new(EP_CRYPTO_KEY *skey, const char *md_alg_name)
{
	EP_CRYPTO_MD *md;
	EP_CRYPTO_MD_ALG *md_alg;
	int istat;

	md_alg = ep_crypto_md_getalg(md_alg_name);
	if (md_alg == NULL)
		return _ep_crypto_error("unknown digest algorithm %s",
				md_alg_name);
	md = EVP_MD_CTX_create();
	if (md == NULL)
		return _ep_crypto_error("cannot create message digest for signing");
	istat = EVP_DigestSignInit(md, NULL, md_alg, NULL, skey);
	if (istat != 1)
	{
		ep_crypto_md_free(md);
		return _ep_crypto_error("cannot initialize digest for signing");
	}
	return md;
}



EP_STAT
ep_crypto_sign_update(EP_CRYPTO_MD *md, void *dbuf, size_t dbufsize)
{
	int istat;

	istat = EVP_DigestSignUpdate(md, dbuf, dbufsize);
	if (istat != 1)
	{
		(void) _ep_crypto_error("cannot update signing digest");
		return EP_STAT_CRYPTO_SIGN;
	}
	return EP_STAT_OK;
}


EP_STAT
ep_crypto_sign_final(EP_CRYPTO_MD *md, void *sbuf, size_t *sbufsize)
{
	int istat;

	istat = EVP_DigestSignFinal(md, sbuf, sbufsize);
	if (istat != 1)
	{
		(void) _ep_crypto_error("cannot finalize signing digest");
		return EP_STAT_CRYPTO_SIGN;
	}
	return EP_STAT_OK;
}


void
ep_crypto_sign_free(EP_CRYPTO_MD *md)
{
	EVP_MD_CTX_destroy(md);
}
