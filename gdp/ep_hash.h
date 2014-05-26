/***********************************************************************
**	Copyright (c) 2008-2010, Eric P. Allman.  All rights reserved.
**	$Id: ep_hash.h 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

#ifndef _EP_HASH_H_
#define _EP_HASH_H_

/***********************************************************************
**
**  HASH HANDLING
**
*/

typedef struct EP_HASH		EP_HASH;
extern const EP_OBJ_CLASS	EpClassHash;

typedef void	(*EP_HASH_FORALL_FUNCP)(
			int keylen,
			const void *key,
			void *val,
			va_list av);
typedef int	(*EP_HASH_HASH_FUNCP)(
			const EP_HASH *const hash,
			const int keylen,
			const void *key);

extern EP_HASH	*ep_hash_new(
			const char *name,
			EP_HASH_HASH_FUNCP hfunc,
			int tabsize);
extern void	ep_hash_free(
			EP_HASH *hash);
extern void	*ep_hash_search(
			const EP_HASH *hash,
			int keylen,
			const void *key);
extern void	*ep_hash_insert(
			EP_HASH *hash,
			int keylen,
			const void *key,
			void *val);
extern void	ep_hash_forall(
			EP_HASH *hash,
			EP_HASH_FORALL_FUNCP func,
			...);

#define EP_HASH_DEFHFUNC	((EP_HASH_HASH_FUNCP) EP_NULL)
#define EP_HASH_DEFHSIZE	0

#endif //_EP_HASH_H_
