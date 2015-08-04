/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep_crypto.h>
#include <ep_dbg.h>

#include <openssl/conf.h>
#include <openssl/err.h>

//static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto", "crypto support");

/*
**  LIBEP CRYPTOGRAPHIC SUPPORT
*/


void
ep_crypto_init(uint32_t flags)
{
	OPENSSL_load_builtin_modules();
	OpenSSL_add_all_algorithms();
}


void *
_ep_crypto_error(const char *msg, ...)
{
	va_list ap;
	FILE *fp = ep_dbg_getfile();
	static bool initialized = false;

	// load openssl error strings if not already done
	if (!initialized)
	{
		ERR_load_crypto_strings();
		initialized = true;
	}

	//XXX should be on some flag (don't print unconditionally)
	va_start(ap, msg);
	ep_dbg_printf("EP Crypto Error: ");
	vfprintf(fp, msg, ap);
	ep_dbg_printf("\n");
	ERR_print_errors_fp(fp);

	return NULL;
}
