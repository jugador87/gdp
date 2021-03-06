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

#include <ep.h>
#include <ep_crypto.h>
#include <ep_dbg.h>

#include <strings.h>

static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto.md", "message digests");


/*
**  Message Digest (Cryptographic Hash) support
**
**	The model is that you create a new digest and then update
**	it as data comes along.
**
**	When done, you finalize it, which gives you the final hash.
**
**	If you have some fixed data at the beginning of multiple
**	data blocks, you can hash that and then "clone" the
**	digest; the clone then picks up where the original part
**	of the digest left off.
*/


/*
**  Convert EP algorithm code to an OpenSSL algorithm.
**
**	Unfortunately this loads in all the algorithms to all
**	processes.  A better implementation would somehow dynamically
**	load them as needed.
*/

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

	  case EP_CRYPTO_MD_RIPEMD160:
		return EVP_ripemd160();

	  default:
		return _ep_crypto_error("unknown digest algorithm %d", md_alg_id);
	}
}


/*
**  Create a new EP Message Digest structure
*/

EP_CRYPTO_MD *
ep_crypto_md_new(int md_alg_id)
{
	const EVP_MD *md_alg;
	EP_CRYPTO_MD *md = EVP_MD_CTX_new();
	int istat;

	md_alg = _ep_crypto_md_getalg_byid(md_alg_id);
	if (md_alg == NULL)
		return NULL;		// error already given
	md = EVP_MD_CTX_new();
	if (md == NULL)
		return _ep_crypto_error("cannot create new message digest");
	istat = EVP_DigestInit_ex(md, md_alg, NULL);
	if (istat != 1)
	{
		EVP_MD_CTX_free(md);
		return _ep_crypto_error("cannot initialize message digest");
	}
	return md;
}


/*
**  Clone (duplicate) a partially completed digest
*/

EP_CRYPTO_MD *
ep_crypto_md_clone(EP_CRYPTO_MD *oldmd)
{
	EP_CRYPTO_MD *md;
	int istat;

	md = EVP_MD_CTX_new();
	if (md == NULL)
		return _ep_crypto_error("cannot create cloned message digest");
	istat = EVP_MD_CTX_copy_ex(md, oldmd);
	if (istat != 1)
		return _ep_crypto_error("cannot clone message digest");

	return md;
}


/*
**  Update (extend) an existing digest
*/

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


/*
**  Finalize and return a message digest.
**
**	The passed in buffer must be large enough to hold the hash.
**	It can be too large, in which case the actual length is
**	returned.  The size will never exceed EP_CRYPTO_MD_MAXSIZE.
*/

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


/*
**  Free a message digest context
*/

void
ep_crypto_md_free(EP_CRYPTO_MD *md)
{
	EVP_MD_CTX_free(md);
}


/*
**  Return the SHA256 of a buffer.
**
**	This can be done using the primitives above, but this makes
**	it easier.
*/

void
ep_crypto_md_sha256(const void *data, size_t dlen, uint8_t *out)
{
	SHA256(data, dlen, out);
}


/*
**  Extract the EP digest type code from a digest context
*/

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

	  case NID_ripemd160:
		mdtype = EP_CRYPTO_MD_RIPEMD160;
		break;

	  default:
		ep_dbg_cprintf(Dbg, 1, "ep_crypto_md_type: unknown md type %d\n",
				EVP_MD_CTX_type(md));
		break;
	}

	return mdtype;
}


/*
**  Return length in bytes of hash output based on algorithm type
**
**	This should really use EVP_MD_size, but that would require
**	passing in an EP_CRYPTO_MD instead of a code.
*/


#define BITS		/ 8

int
ep_crypto_md_len(int md_alg_id)
{
	switch (md_alg_id)
	{
	  case EP_CRYPTO_MD_SHA1:
	  case EP_CRYPTO_MD_RIPEMD160:
		return 160 BITS;

	  case EP_CRYPTO_MD_SHA224:
		return 224 BITS;

	  case EP_CRYPTO_MD_SHA256:
		return 256 BITS;

	  case EP_CRYPTO_MD_SHA384:
		return 384 BITS;

	  case EP_CRYPTO_MD_SHA512:
		return 512 BITS;

	  default:
		return 0;
	}
}


/*
**  Convert message digest names to and from internal EP codes.
*/

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
	{ "ripemd160",		EP_CRYPTO_MD_RIPEMD160,		},
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
