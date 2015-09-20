/* vim: set ai sw=8 sts=8 ts=8 :*/

#include <ep/ep_crypto.h>

struct ep_crypto_cipher_ctx
{
	EP_CRYPTO_KEY	*key;		// symmetric encryption/decryption key
	EVP_CIPHER_CTX	ctx;		// OpenSSL cipher context
//	uint8_t		iv[EVP_MAX_IV_LENGTH];
};


static const EVP_CIPHER *
evp_cipher_type(uint32_t cipher)
{
	switch (cipher)
	{
	  case EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_CBC:
		return EVP_aes_128_cbc();
	  case EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_CFB:
		return EVP_aes_128_cfb();
	  case EP_CRYPTO_SYMKEY_AES128 | EP_CRYPTO_MODE_OFB:
		return EVP_aes_128_ofb();

	  case EP_CRYPTO_SYMKEY_AES192 | EP_CRYPTO_MODE_CBC:
		return EVP_aes_192_cbc();
	  case EP_CRYPTO_SYMKEY_AES192 | EP_CRYPTO_MODE_CFB:
		return EVP_aes_192_cfb();
	  case EP_CRYPTO_SYMKEY_AES192 | EP_CRYPTO_MODE_OFB:
		return EVP_aes_192_ofb();

	  case EP_CRYPTO_SYMKEY_AES256 | EP_CRYPTO_MODE_CBC:
		return EVP_aes_256_cbc();
	  case EP_CRYPTO_SYMKEY_AES256 | EP_CRYPTO_MODE_CFB:
		return EVP_aes_256_cfb();
	  case EP_CRYPTO_SYMKEY_AES256 | EP_CRYPTO_MODE_OFB:
		return EVP_aes_256_ofb();
	}

	return _ep_crypto_error("unknown cipher type 0x%x", cipher);
}


EP_CRYPTO_CIPHER_CTX *
ep_crypto_cipher_new(
		uint32_t cipher,
		uint8_t *key,
		uint8_t *iv,
		bool enc)
{
	EP_CRYPTO_CIPHER_CTX *ctx;
	const EVP_CIPHER *ciphertype;
	ENGINE *engine = NULL;

	ciphertype = evp_cipher_type(cipher);
	if (ciphertype == NULL)
		return NULL;

	ctx = ep_mem_zalloc(sizeof *ctx);
	EVP_CIPHER_CTX_init(&ctx->ctx);
	if (EVP_CipherInit_ex(&ctx->ctx, ciphertype, engine, key, iv, enc) <= 0)
	{
		ep_crypto_cipher_free(ctx);
		return _ep_crypto_error("cannot initialize for encryption");
	}

	return ctx;
}


void
ep_crypto_cipher_free(EP_CRYPTO_CIPHER_CTX *ctx)
{
	if (EVP_CIPHER_CTX_cleanup(&ctx->ctx) <= 0)
		_ep_crypto_error("cannot cleanup cipher context");
	ep_mem_free(ctx);
}


EP_STAT
ep_crypto_cipher_crypt(
		EP_CRYPTO_CIPHER_CTX *ctx,
		void *in,
		size_t inlen,
		void *out,
		size_t outlen)
{
	int olen;
	int rval;

	// must allow room for final padding in output buffer
	if (outlen < inlen + EVP_CIPHER_CTX_key_length(&ctx->ctx))
	{
		// potential buffer overflow
		_ep_crypto_error("cp_crypto_cipher_crypt: short output buffer "
				"(%d < %d + %d)",
			outlen, inlen, EVP_CIPHER_CTX_key_length(&ctx->ctx));
		return EP_STAT_BUF_OVERFLOW;
	}

	if (EVP_CipherUpdate(&ctx->ctx, out, &olen, in, inlen) <= 0)
	{
		_ep_crypto_error("cannot encrypt/decrypt");
		return EP_STAT_CRYPTO_CIPHER;
	}
	rval = olen;
	out += olen;
	if (EVP_CipherFinal_ex(&ctx->ctx, out, &olen) <= 0)
	{
		_ep_crypto_error("cannot finalize encrypt/decrypt");
		return EP_STAT_CRYPTO_CIPHER;
	}

	rval += olen;
	EP_ASSERT_ENSURE(rval >= 0);
	return EP_STAT_FROM_INT(rval);
}


EP_STAT
ep_crypto_cipher_cryptblock(
		EP_CRYPTO_CIPHER_CTX *ctx,
		void *in,
		size_t inlen,
		void *out,
		size_t outlen)
{
	int olen;

	if (EVP_CipherUpdate(&ctx->ctx, out, &olen, in, inlen) <= 0)
	{
		_ep_crypto_error("cannot encrypt/decrypt");
		return EP_STAT_CRYPTO_CIPHER;
	}

	EP_ASSERT_ENSURE(olen >= 0 && olen <= outlen);
	return EP_STAT_FROM_INT(olen);
}


EP_STAT
ep_crypto_cipher_finish(
		EP_CRYPTO_CIPHER_CTX *ctx,
		void *out,
		size_t outlen)
{
	int olen;

	// allow room for possible final padding
	if (outlen < EVP_CIPHER_CTX_key_length(&ctx->ctx))
	{
		// potential buffer overflow
		_ep_crypto_error("cp_crypto_cipher_finish: short output buffer (%d < %d)",
				outlen, EVP_CIPHER_CTX_key_length(&ctx->ctx));
		return EP_STAT_BUF_OVERFLOW;
	}

	if (EVP_CipherFinal_ex(&ctx->ctx, out, &olen) <= 0)
	{
		_ep_crypto_error("cannot finalize encrypt/decrypt");
		return EP_STAT_CRYPTO_CIPHER;
	}

	EP_ASSERT_ENSURE(olen >= 0);
	return EP_STAT_FROM_INT(olen);
}
