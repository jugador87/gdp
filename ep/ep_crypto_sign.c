/* vim: set ai sw=8 sts=8 ts=8 : */

/*
**  Do cryptographic signatures
**
**	Note that the internal structure while computing a signature
**	is an EP_CRYPTO_MD; thus you may see a mix of calls to (for
**	example) ep_crypto_md_clone and ep_crypto_sign_final.
*/

#include <ep/ep.h>
#include <ep_crypto.h>
#include <ep_dbg.h>

//static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto.sign", "cryptographic signatures");


/*
**  Create a new signing context.  Note that this returns a message
**  digest type; there isn't a separate type for signing.
*/

EP_CRYPTO_MD *
ep_crypto_sign_new(EP_CRYPTO_KEY *skey, int md_alg_id)
{
	EP_CRYPTO_MD *md;
	const EVP_MD *md_alg;
	int istat;

	md_alg = _ep_crypto_md_getalg_byid(md_alg_id);
	if (md_alg == NULL)
		return _ep_crypto_error("unknown digest algorithm %d",
				md_alg_id);
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


/*
**  Update a signing context.  This adds data to be signed.
*/

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


/*
**  Finalize a signing context.
**
**	This returns the signature in sbuf and sbufsize, with
**	*sbufsize set to the size of sbuf on call, and modified
**	to be the number of bytes actually used.
**
**	The context cannot be used again.
*/

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


/*
**  Free the signing context.
*/

void
ep_crypto_sign_free(EP_CRYPTO_MD *md)
{
	EVP_MD_CTX_destroy(md);
}
