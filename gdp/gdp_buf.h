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

#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 199900L
inline gdp_buf_t *
gdp_buf_new(void)
{
	return evbuffer_new();
}

inline int
gdp_buf_reset(gdp_buf_t *b)
{
	return evbuffer_drain(b, evbuffer_get_length(b));
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

inline int
gdp_buf_drain(gdp_buf_t *buf, size_t sz)
{
	return evbuffer_drain(buf, sz);
}

inline unsigned char *
gdp_buf_getptr(gdp_buf_t *buf, size_t sz)
{
	return evbuffer_pullup(buf, sz);
}

inline int
gdp_buf_write(gdp_buf_t *buf, void *in, size_t sz)
{
	return evbuffer_add(buf, in, sz);
}

inline int
gdp_buf_copy(gdp_buf_t *ibuf, gdp_buf_t *obuf)
{
	return evbuffer_add_buffer(obuf, ibuf);
}

#else

#define gdp_buf_new()			evbuffer_new()
#define gdp_buf_reset(b)		evbuffer_drain(b, evbuffer_get_length(b))
#define gdp_buf_free(b)			evbuffer_free(b)
#define gdp_buf_setlock(b, m)	evbuffer_enable_locking(b, m)
#define gdp_buf_getlength(b)	evbuffer_get_length(b)
#define gdp_buf_read(b, o, z)	evbuffer_remove(b, o, z)
#define gdp_buf_peek(b, o, z)	evbuffer_copyout(b, o, z)
#define gdp_buf_drain(b, z)		evbuffer_drain(b, z)
#define gdp_buf_getptr(b, z)	evbuffer_pullup(b, z)
#define gdp_buf_write(b, i, z)	evbuffer_add(b, i, z)
#define gdp_buf_copy(i, o)		evbuffer_add_buffer(o, i)

#endif

#define gdp_buf_printf			evbuffer_add_printf

extern void		gdp_buf_dump(gdp_buf_t *buf, FILE *fp);

#endif // _GDP_BUF_H_