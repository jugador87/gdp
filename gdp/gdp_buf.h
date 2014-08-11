/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_BUF.H --- data buffers for the GDP
*/

#ifndef _GDP_BUF_H_
#define _GDP_BUF_H_

#include <event2/event.h>
#include <event2/buffer.h>
#include <ep/ep_thr.h>

typedef struct evbuffer	gdp_buf_t;

inline gdp_buf_t *
gdp_buf_new(void)
{
	return evbuffer_new();
}

inline void
gdp_buf_free(gdp_buf_t *b)
{
	evbuffer_free(b);
}

inline void
gdp_buf_setlock(gdp_buf_t *buf, EP_THR_MUTEX *m)
{
	evbuffer_enable_locking(buf, m);
}

inline size_t
gdp_buf_getlength(gdp_buf_t *buf)
{
	return evbuffer_get_length(buf);
}

inline size_t
gdp_buf_read(gdp_buf_t *buf, void *out, size_t sz)
{
	return evbuffer_remove(buf, out, sz);
}

inline size_t
gdp_buf_peek(gdp_buf_t *buf, void *out, size_t sz)
{
	return evbuffer_copyout(buf, out, sz);
}

inline size_t
gdp_buf_drain(gdp_buf_t *buf, size_t sz)
{
	return evbuffer_drain(buf, sz);
}

inline unsigned char *
gdp_buf_getptr(gdp_buf_t *buf, size_t sz)
{
	return evbuffer_pullup(buf, sz);
}

inline size_t
gdp_buf_write(gdp_buf_t *buf, void *in, size_t sz)
{
	return evbuffer_add(buf, in, sz);
}

#define gdp_buf_printf			evbuffer_add_printf

#endif // _GDP_BUF_H_
