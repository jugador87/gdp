/* vim: set ai sw=8 sts=8 :*/

/*
**  EP.H -- general definitions for EP library.
**
**  	A lot of this is just to override some of the full library names.
**  	Some should probably be broken out into other headers.
*/

#ifndef _EP_H_
#define _EP_H_

#include <ep/ep_conf.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <ep/ep_assert.h>
#include <ep/ep_stat.h>

//XXX hacks for non-libep settings
#define EP_SRC_ID(x)


// yes, I know this doesn't work; give me a break
typedef int			EP_MUTEX;
#define EP_MUTEX_LOCK(m)	EP_ASSERT((m)++ == 0)
#define EP_MUTEX_UNLOCK(m)	EP_ASSERT(--(m) == 0)
#define EP_MUTEX_IS_LOCKED(m)	((m) != 0)
#define EP_MUTEX_INIT		= 0

void	*ep_mem_malloc(size_t);
void	*ep_mem_zalloc(size_t);
char	*ep_mem_strdup(const char *);
void	ep_mem_free(void *);

#define EP_GEN_DEADBEEF		((void *) 0xDEADBEEF)
#define EP_ASSERT_POINTER_VALID(p) \
				EP_ASSERT((p) != NULL && (p) != EP_GEN_DEADBEEF)

#define EP_UT_BITSET(bit, word)	(((bit) & (word)) != 0)

extern int		ep_adm_getintparam(const char *name, int def);
extern long		ep_adm_getlongparam(const char *name, long def);
extern bool		ep_adm_getboolparam(const char *name, bool def);
extern const char	*ep_adm_getstrparam(const char *name, const char *def);

extern FILE		*ep_fopensmem(void *buf, size_t bufsz, const char *mode);

#endif // _EP_H_
