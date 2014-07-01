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
// ideally this would check valid pointers for this architecture
#define EP_ASSERT_POINTER_VALID(p) \
				EP_ASSERT((p) != NULL && (p) != EP_GEN_DEADBEEF)

#define EP_UT_BITSET(bit, word)	(((bit) & (word)) != 0)

extern int		ep_adm_getintparam(	// get integer param value
				const char *name,	// name of param
				int def);		// default value
extern long		ep_adm_getlongparam(	// get long param value
				const char *name,	// name of param
				long def);		// default value
extern bool		ep_adm_getboolparam(	// get boolean param value
				const char *name,	// name of param
				bool def);		// default value
extern const char	*ep_adm_getstrparam(	// get string param value
				const char *name,	// name of param
				const char *def);	// default value

extern FILE		*ep_fopensmem(		// open a static memory buffer
				void *buf,		// buffer
				size_t bufsz,		// size of buffer
				const char *mode);	// mode, e.g., r, w
extern size_t		ep_fread_unlocked(	// unlocked version of fread
				void *buf,		// buffer area
				size_t sz,		// size of one item
				size_t n,		// number of items
				FILE *fp);		// file to read
extern size_t		ep_fwrite_unlocked(	// unlocked version of fwrite
				void *buf,		// buffer area
				size_t sz,		// size of one item
				size_t n,		// number of items
				FILE *fp);		// file to write

#endif // _EP_H_
