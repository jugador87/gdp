/* vim: set ai sw=4 sts=4 ts=4 : */

#include <gdp/gdp_priv.h>

/*
**  Verify a GCL datum including appropriate metadata.
**  Signatures cover:
**  	* The name of the log.
**  	* The serialized metadata for this log.  This will include
**  	  the public key.
**  	* The record number in network byte order.
**  	* The contents of the record.
**
**  The hash for everything before the record number can be pre-computed
**  to speed things up.  This is done when the GCL is opened.
*/

XXXTYPE
gdp_rec_verify_init(gdp_gcl_t *gcl, gdp_pkey_t *pkey, XXX)
{
	EP_CRYPTO_MD *md;

	md_type = ep_crypto_md_get(NULL);
	result = ep_crypto_vrfy_init(md, md_alg, pkey);
}

#define PUT64(v) \
		{ \
			*pbp++ = ((v) >> 56) & 0xff; \
			*pbp++ = ((v) >> 48) & 0xff; \
			*pbp++ = ((v) >> 40) & 0xff; \
			*pbp++ = ((v) >> 32) & 0xff; \
			*pbp++ = ((v) >> 24) & 0xff; \
			*pbp++ = ((v) >> 16) & 0xff; \
			*pbp++ = ((v) >>  8) & 0xff; \
			*pbp++ = ((v) >>  0) & 0xff; \
		}

XXXTYPE
gdp_rec_verify(gdp_gcl_t *gcl, void *msg, size_t mlen)
{
	gdp_md_ctx_t *md_ctx;
	uint8_t recnobuf[8];
	uint8_t *pbp;

	// copy the context so we can put extra data
	// ... gives us a new context based on the old one
	md_ctx = ep_crypto_mdctx_clone(gdp->md_ctx);

	// get the record number in network byte order
	pbp = recnobuf;
	PUT64(gdp->gdp_recno);

	ep_crypto_update(md_ctx, recnobuf, sizeof recnobuf);
	ep_crypto_update(md_ctx, msg, mlen);

	result = ep_crypto_verify(md_ctx, gdp->pkey, sigbuf, sizeof sigbuf);
}
