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
		return _ep_crypto_error("ep_crypto_sign_new: "
				"unknown digest algorithm %d",
				md_alg_id);
	md = EVP_MD_CTX_create();
	if (md == NULL)
		return _ep_crypto_error("ep_crypto_sign_new: "
				"cannot create message digest for signing");
	istat = EVP_DigestSignInit(md, NULL, md_alg, NULL, skey);
	if (istat != 1)
	{
		ep_crypto_md_free(md);
		return _ep_crypto_error("ep_crypto_sign_new: "
				"cannot initialize digest for signing");
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
		(void) _ep_crypto_error("ep_crypto_sign_update: "
				"cannot update signing digest");
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
		(void) _ep_crypto_error("ep_crypto_sign_final: "
				"cannot finalize signing digest");
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
