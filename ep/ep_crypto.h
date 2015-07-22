/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2014, Eric P. Allman.  All rights reserved.
***********************************************************************/

#ifndef _EP_CRYPTO_H_
#define _EP_CRYPTO_H_

# include <ep/ep.h>
# include <ep/ep_statcodes.h>

# include <openssl/evp.h>

/*
**  General defines
*/

# define EP_CRYPTO_MAX_PUB_KEY	(1024 * 8)
# define EP_CRYPTO_MAX_SEC_KEY	(1024 * 8)
# define EP_CRYPTO_MAX_DIGEST	(512 / 8)

/*
**  Message Digests
*/

# define EP_CRYPTO_KEY		EVP_PKEY
# define EP_CRYPTO_MD		EVP_MD_CTX
# define EP_CRYPTO_MD_ALG	const EVP_MD

// digest algorithms
# define EP_CRYPTO_MD_NULL	0
# define EP_CRYPTO_MD_SHA1	1
# define EP_CRYPTO_MD_SHA224	2
# define EP_CRYPTO_MD_SHA256	3
# define EP_CRYPTO_MD_SHA384	4
# define EP_CRYPTO_MD_SHA512	5

EP_CRYPTO_MD_ALG	*ep_crypto_md_getalg(
				const char *algname);
EP_CRYPTO_MD		*ep_crypto_md_new(
				const char *md_alg_name);
EP_CRYPTO_MD		*ep_crypto_md_clone(
				EP_CRYPTO_MD *base_md);
void			ep_crypto_md_free(
				EP_CRYPTO_MD *md);
EP_STAT			ep_crypto_md_update(
				EP_CRYPTO_MD *md,
				void *data,
				size_t dsize);
EP_STAT			ep_crypto_md_final(
				EP_CRYPTO_MD *md,
				void *dbuf,
				size_t *dbufsize);
int			ep_crypto_md_type(
				EP_CRYPTO_MD *md);

/*
**  Signing and Verification
*/

# define EP_CRYPTO_MAX_SIG	(1024 * 8)

EP_CRYPTO_MD		*ep_crypto_sign_new(
				EP_CRYPTO_KEY *pkey,
				const char *md_alg_name);
void			ep_crypto_sign_free(
				EP_CRYPTO_MD *md);
EP_STAT			ep_crypto_sign_update(
				EP_CRYPTO_MD *md,
				void *dbuf,
				size_t dbufsize);
EP_STAT			ep_crypto_sign_final(
				EP_CRYPTO_MD *md,
				void *sbuf,
				size_t *sbufsize);

EP_CRYPTO_MD		*ep_crypto_vrfy_new(
				EP_CRYPTO_KEY *pkey,
				const char *md_alg_name);
void			ep_crypto_vrfy_free(
				EP_CRYPTO_MD *md);
EP_STAT			ep_crypto_vrfy_update(
				EP_CRYPTO_MD *md,
				void *dbuf,
				size_t dbufsize);
EP_STAT			ep_crypto_vrfy_final(
				EP_CRYPTO_MD *md,
				void *obuf,
				size_t obufsize);


/*
**  Error Handling
*/

void	*_ep_crypto_error(const char *msg, ...);

#endif // _EP_CRYPTO_H_
