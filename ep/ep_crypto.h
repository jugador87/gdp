/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2014, Eric P. Allman.  All rights reserved.
***********************************************************************/

#ifndef _EP_CRYPTO_H_
#define _EP_CRYPTO_H_

# include <ep/ep.h>
# include <ep/ep_statcodes.h>


/*
**  At the moment this wraps openssl, but we could conceivably
**  in the future switch to another package, e.g., NaCl.
*/

# include <openssl/evp.h>



/*
**  Configuration
*/

# ifndef _EP_CRYPTO_INCLUDE_RSA
#  define _EP_CRYPTO_INCLUDE_RSA	1
# endif
# ifndef _EP_CRYPTO_INCLUDE_DSA
#  define _EP_CRYPTO_INCLUDE_DSA	1
# endif
# ifndef _EP_CRYPTO_INCLUDE_EC
#  define _EP_CRYPTO_INCLUDE_EC		0
# endif
# ifndef _EP_CRYPTO_INCLUDE_DH
#  define _EP_CRYPTO_INCLUDE_DH		0
# endif
# ifndef _EP_CRYPTO_INCLUDE_DER
#  define _EP_CRYPTO_INCLUDE_DER	1	// ASN.1
# endif

/*
**  General defines
*/

# define EP_CRYPTO_MAX_PUB_KEY	(1024 * 8)
# define EP_CRYPTO_MAX_SEC_KEY	(1024 * 8)
# define EP_CRYPTO_MAX_DIGEST	(512 / 8)
# define EP_CRYPTO_MAX_DER	(1024 * 8)	//XXX should add a slop factor

# define EP_CRYPTO_KEY		EVP_PKEY


/*
**  Ciphers
*/

# define EP_CRYPTO_CIPHER_NONE		0


/*
**  Key Management
*/

// on-disk key formats
# define EP_CRYPTO_KEYFORM_UNKNOWN	0	// error
# define EP_CRYPTO_KEYFORM_PEM		1	// PEM (text)
# define EP_CRYPTO_KEYFORM_DER		2	// DER (binary ASN.1)

// key types
# define EP_CRYPTO_KEYTYPE_UNKNOWN	0	// error
# define EP_CRYPTO_KEYTYPE_RSA		1	// RSA
# define EP_CRYPTO_KEYTYPE_DSA		2	// DSA
# define EP_CRYPTO_KEYTYPE_EC		3	// Elliptic curve
# define EP_CRYPTO_KEYTYPE_DH		4	// Diffie-Hellman

// flag bits
# define EP_CRYPTO_F_PUBLIC		0x0000	// public key (no flags set)
# define EP_CRYPTO_F_SECRET		0x0001	// secret key

// limits
# define EP_CRYPTO_KEY_MINLEN_RSA	1024

EP_CRYPTO_KEY		*ep_crypto_key_create(
				int keytype,
				int keylen,
				int keyexp,
				const char *curve);
EP_CRYPTO_KEY		*ep_crypto_key_read_file(
				const char *filename,
				int keytype,
				int keyform,
				uint32_t flags);
EP_CRYPTO_KEY		*ep_crypto_key_read_fp(
				FILE *fp,
				const char *filename,
				int keytype,
				int keyform,
				uint32_t flags);
EP_CRYPTO_KEY		*ep_crypto_key_read_mem(
				void *buf,
				size_t buflen,
				int keytype,
				int keyform,
				uint32_t flags);
EP_STAT			ep_crypto_key_write_file(
				EP_CRYPTO_KEY *key,
				const char *filename,
				int keyform,
				int cipher,
				uint32_t flags);
EP_STAT			ep_crypto_key_write_fp(
				EP_CRYPTO_KEY *key,
				FILE *fp,
				int keyform,
				int cipher,
				uint32_t flags);
EP_STAT			ep_crypto_key_write_mem(
				EP_CRYPTO_KEY *key,
				void *buf,
				size_t bufsize,
				int keyform,
				int cipher,
				uint32_t flags);
void			ep_crypto_key_free(
				EP_CRYPTO_KEY *key);
int			ep_crypto_keyform_byname(
				const char *fmt);
int			ep_crypto_key_id_fromkey(
				EP_CRYPTO_KEY *key);
int			ep_crypto_keytype_byname(
				const char *alg_name);


/*
**  Message Digests
*/

# define EP_CRYPTO_MD		EVP_MD_CTX

// digest algorithms (no more than 4 bits)
# define EP_CRYPTO_MD_NULL	0
# define EP_CRYPTO_MD_SHA1	1
# define EP_CRYPTO_MD_SHA224	2
# define EP_CRYPTO_MD_SHA256	3
# define EP_CRYPTO_MD_SHA384	4
# define EP_CRYPTO_MD_SHA512	5

int			ep_crypto_md_alg_byname(
				const char *algname);
EP_CRYPTO_MD		*ep_crypto_md_new(
				int md_alg_id);
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

// private
const EVP_MD		*_ep_crypto_md_getalg_byid(
				int md_alg_id);

/*
**  Signing and Verification
*/

# define EP_CRYPTO_MAX_SIG	(1024 * 8)

EP_CRYPTO_MD		*ep_crypto_sign_new(
				EP_CRYPTO_KEY *skey,
				int md_alg_id);
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
				int md_alg_id);
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
