/* vim: set ai sw=4 sts=4 ts=4 : */

#include <gdp/gdp_priv.h>

/*
**  Sign a GCL datum including appropriate metadata.
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
gdp_rec_sign(gdp_gcl_t *gcl, void *msg, size_t mlen)
{
	_gdp_sign(msg, mlen, gdp->md_ctx, gdp->skey, sigbuf, sizeof sigbuf);
}
