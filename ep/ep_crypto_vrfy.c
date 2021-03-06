/* vim: set ai sw=8 sts=8 ts=8 : */

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**  ----- END LICENSE BLOCK -----
***********************************************************************/

/*
**  Wrappers for cryptographic signature verification
**
**	These are direct analogues to the signing routines.
*/

#include <ep/ep.h>
#include <ep_crypto.h>


/*
**  Create a new cryptographic verification context.
*/

EP_CRYPTO_MD *
ep_crypto_vrfy_new(EP_CRYPTO_KEY *pkey, int md_alg_id)
{
	EP_CRYPTO_MD *md;
	const EVP_MD *md_alg;
	int istat;

	md_alg = _ep_crypto_md_getalg_byid(md_alg_id);
	if (md_alg == NULL)
		return _ep_crypto_error("unknown digest algorithm %d",
				md_alg_id);
	md = EVP_MD_CTX_new();
	if (md == NULL)
		return _ep_crypto_error("cannot create message digest for verification");
	istat = EVP_DigestVerifyInit(md, NULL, md_alg, NULL, pkey);
	if (istat != 1)
	{
		ep_crypto_md_free(md);
		return _ep_crypto_error("cannot initialize digest for verification");
	}
	return md;
}


/*
**  Add data to verification context.
*/

EP_STAT
ep_crypto_vrfy_update(EP_CRYPTO_MD *md, void *dbuf, size_t dbufsize)
{
	int istat;

	istat = EVP_DigestVerifyUpdate(md, dbuf, dbufsize);
	if (istat != 1)
	{
		(void) _ep_crypto_error("cannot update verify digest");
		return EP_STAT_CRYPTO_VRFY;
	}
	return EP_STAT_OK;
}


/*
**  Finalize a verification context.
**
**	The caller passes in an existing signature.  This routine
**	checks to make sure that signature matches the data that has
**	been passed into the context.
*/

EP_STAT
ep_crypto_vrfy_final(EP_CRYPTO_MD *md, void *obuf, size_t obufsize)
{
	int istat;

	istat = EVP_DigestVerifyFinal(md, obuf, obufsize);
	if (istat == 1)
	{
		// success
		return EP_STAT_OK;
	}
	else if (istat == 0)
	{
		// signature verification failure
		(void) _ep_crypto_error("signature invalid");
		return EP_STAT_CRYPTO_BADSIG;
	}
	else
	{
		// more serious error
		(void) _ep_crypto_error("cannot finalize verify digest");
		return EP_STAT_CRYPTO_VRFY;
	}
}


/*
**  Free the verification context.
*/

void
ep_crypto_vrfy_free(EP_CRYPTO_MD *md)
{
	EVP_MD_CTX_free(md);
}
