/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_BUF --- data buffers for the GDP
*/

#include "gdp.h"
#include "gdp_buf.h"

#include <stdarg.h>

gdp_buf_t *
gdp_buf_new(void)
{
	return evbuffer_new();
}

int
gdp_buf_reset(gdp_buf_t *b)
{
	return evbuffer_drain(b, evbuffer_get_length(b));
}

void
gdp_buf_free(gdp_buf_t *b)
{
	evbuffer_free(b);
}

void
gdp_buf_setlock(gdp_buf_t *buf, EP_THR_MUTEX *m)
{
	evbuffer_enable_locking(buf, m);
}

size_t
gdp_buf_getlength(gdp_buf_t *buf)
{
	return evbuffer_get_length(buf);
}

size_t
gdp_buf_read(gdp_buf_t *buf, void *out, size_t sz)
{
	return evbuffer_remove(buf, out, sz);
}

size_t
gdp_buf_peek(gdp_buf_t *buf, void *out, size_t sz)
{
	return evbuffer_copyout(buf, out, sz);
}

int
gdp_buf_drain(gdp_buf_t *buf, size_t sz)
{
	return evbuffer_drain(buf, sz);
}

unsigned char *
gdp_buf_getptr(gdp_buf_t *buf, size_t sz)
{
	return evbuffer_pullup(buf, sz);
}

int
gdp_buf_write(gdp_buf_t *buf, void *in, size_t sz)
{
	return evbuffer_add(buf, in, sz);
}

int
gdp_buf_printf(gdp_buf_t *buf, const char *fmt, ...)
{
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = evbuffer_add_vprintf(buf, fmt, ap);
	va_end(ap);
	return res;
}

int
gdp_buf_copy(gdp_buf_t *ibuf, gdp_buf_t *obuf)
{
	return evbuffer_add_buffer(obuf, ibuf);
}

void
gdp_buf_dump(gdp_buf_t *buf, FILE *fp)
{
	fprintf(fp, "gdp_buf @ %p: len=%zu\n",
			buf, gdp_buf_getlength(buf));
}
