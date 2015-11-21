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

////////////////////////////////////////////////////////////////////////
//
//  MEMORY ALLOCATION: HEAPS
//
////////////////////////////////////////////////////////////////////////

#ifndef _EP_MEM_H_
#define _EP_MEM_H_

#ifndef EP_MEM_DEBUG
# define EP_MEM_DEBUG	1
#endif

struct ep_malloc_functions
{
	void	*(*m_malloc)(size_t);
	void	*(*m_realloc)(void*, size_t);
	void	*(*m_valloc)(size_t);
	void	(*m_free)(void*);
};

// flag bits for allocation
#define EP_MEM_F_FAILOK		0x00000001	// return NULL on error
#define EP_MEM_F_ZERO		0x00000004	// zero memory before return
#define EP_MEM_F_TRASH		0x00000008	// return random-filled memory
#define EP_MEM_F_ALIGN		0x00000010	// return aligned memory (maybe)
#define EP_MEM_F_WAIT		0x00000020	// wait for memory on failure

#if EP_MEM_DEBUG
#  define _EP_MEM_FILE_LINE_	__FILE__, __LINE__
#else
#  define _EP_MEM_FILE_LINE_	NULL, 0
#endif

# define ep_mem_malloc(size)	ep_mem_ialloc((size), NULL, \
					0, \
					_EP_MEM_FILE_LINE_)
# define ep_mem_zalloc(size)	ep_mem_ialloc((size), NULL, \
					EP_MEM_F_ZERO, \
					_EP_MEM_FILE_LINE_)
# define ep_mem_ralloc(size)	ep_mem_ialloc((size), NULL, \
					EP_MEM_F_TRASH, \
					_EP_MEM_FILE_LINE_)
# define ep_mem_ealloc(size)	ep_mem_ialloc((size), NULL, \
					EP_MEM_F_FAILOK, \
					_EP_MEM_FILE_LINE_)
# define ep_mem_falloc(size, flags) \
				ep_mem_ialloc((size), NULL, (flags), \
					_EP_MEM_FILE_LINE_)
# define ep_mem_erealloc(curmem, size) \
				ep_mem_ialloc((size), \
					(curmem), EP_MEM_F_FAILOK, \
					_EP_MEM_FILE_LINE_)
# define ep_mem_realloc(curmem, size) \
				ep_mem_ialloc((size), (curmem), 0, \
					_EP_MEM_FILE_LINE_)
# define ep_mem_strdup(s)	ep_mem_istrdup(s, -1, 0, \
					_EP_MEM_FILE_LINE_)
# define ep_mem_fstrdup(s, flags) \
				ep_mem_istrdup(s, -1, (flags), \
					_EP_MEM_FILE_LINE_)
# define ep_mem_strndup(s, l)	ep_mem_istrdup(s, l, 0, \
					_EP_MEM_FILE_LINE_)
# define ep_mem_mfree(p)	ep_mem_free(p)

extern void	*ep_mem_ialloc(		// allocate from heap w/ flags
			size_t nbytes,			// bytes to alloc
			void *curmem,			// if reallocing
			uint32_t flags,			// mod flags
			const char *file,		// dbg: file name
			int line);			// dbg: line number
extern char	*ep_mem_istrdup(		// dup string from heap w/ flags
			const char *s,			// string to copy
			ssize_t slen,			// max length of s
			uint32_t flags,			// action modifiers
			const char *file,		// dbg: file name
			int line);			// dbg: line number
extern void	ep_mem_mfree(			// free from heap
			void *p);			// space to free

// for function pointers
extern void	*ep_mem_malloc_f(size_t nbytes);
extern void	*ep_mem_zalloc_f(size_t nbytes);
extern void	*ep_mem_ralloc_f(size_t nbytes);

extern void	ep_mem_trash(			// trash memory
			void *p,			// ptr to memory
			size_t nbytes);			// number of trash bytes

#endif // _EP_MEM_H_
