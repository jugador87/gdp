/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**  ----- END LICENSE BLOCK -----
***********************************************************************/

#ifndef _EP_HASH_H_
#define _EP_HASH_H_

/***********************************************************************
**
**  HASH HANDLING
**
*/

typedef struct EP_HASH		EP_HASH;

typedef void	(*EP_HASH_FORALL_FUNCP)(
			size_t keylen,
			const void *key,
			void *val,
			va_list av);
typedef int	(*EP_HASH_HASH_FUNCP)(
			const EP_HASH *const hash,
			const size_t keylen,
			const void *key);

extern EP_HASH	*ep_hash_new(
			const char *name,
			EP_HASH_HASH_FUNCP hfunc,
			int tabsize);
extern void	ep_hash_free(
			EP_HASH *hash);
extern void	*ep_hash_search(
			EP_HASH *hash,
			size_t keylen,
			const void *key);
extern void	*ep_hash_insert(
			EP_HASH *hash,
			size_t keylen,
			const void *key,
			void *val);
extern void	*ep_hash_delete(
			EP_HASH *hash,
			size_t keylen,
			const void *key);
extern void	ep_hash_forall(
			EP_HASH *hash,
			EP_HASH_FORALL_FUNCP func,
			...);

#define EP_HASH_DEFHFUNC	((EP_HASH_HASH_FUNCP) NULL)
#define EP_HASH_DEFHSIZE	0

#endif //_EP_HASH_H_
