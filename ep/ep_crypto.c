/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep_crypto.h>
#include <ep_dbg.h>

#include <openssl/err.h>

//static EP_DBG	Dbg = EP_DBG_INIT("libep.crypto", "crypto support");

/*
**  EP CRYPTOGRAPHIC SUPPORT
*/

void *
_ep_crypto_error(const char *msg, ...)
{
	va_list ap;
	FILE *fp = ep_dbg_getfile();

	//XXX should be on some flag (don't print unconditionally)
	va_start(ap, msg);
	ep_dbg_printf("EP Crypto Error: ");
	vfprintf(fp, msg, ap);
	ep_dbg_printf("\n");
	ERR_print_errors_fp(fp);

	return NULL;
}
