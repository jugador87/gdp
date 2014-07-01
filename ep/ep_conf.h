/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2014, Eric P. Allman.  All rights reserved.
**	$Id: ep_conf.h 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

#ifndef _EP_CONF_H_
# define _EP_CONF_H_

/*
**  Local configuration.  There will be one of these for any platform
**  that needs tweaks.
**
**  Possible options:
**	EP_OSCF_HAS_INTTYPES_H
**		Set if <inttypes.h> exists
**	EP_OSCF_HAS_STDBOOL_H
**		Set if <stdbool.h> exists
**	EP_OSCF_HAS_STDINT_H
**		Set if <stdint.h> exists
**	EP_OSCF_HAS_STDLIB_H
**		Set if <stdlib.h> exists
**	EP_OSCF_HAS_STRING_H
**		Set if <string.h> exists
**	EP_OSCF_HAS_SYS_TYPES_H
**		Set if <sys/types.h> exists
**	EP_OSCF_HAS_UNISTD_H
**		Set if <unistd.h> exists
**	EP_OSCF_HAS_UCHAR_T
**		Set if uchar_t is defined
**	EP_OSCF_HAS_SYS_CDEFS_H
**		Set if <sys/cdefs.h> exists
**	EP_OSCF_HAS_STRLCPY
**		Set if strlcpy(3) is available
**	EP_OSCF_HAS_GETPROGNAME
**		Set if getprogname(3) is available
**	EP_OSCF_MEM_PAGESIZE
**		Memory page size; defaults to getpagesize()
**	EP_OSCF_MEM_ALIGNMENT
**		Best generic memory alignment; defaults to 4
**	EP_OSCF_SYSTEM_MALLOC
**		System memory allocation routine; defaults to "malloc"
**	EP_OSCF_SYSTEM_VALLOC
**		System aligned memory allocation routine; defaults to "malloc"
**	EP_OSCF_SYSTEM_REALLOC
**		System memory reallocation routine; defaults to "realloc"
**	EP_OSCF_SYSTEM_MFREE
**		System memory free routine; defaults to "free"
**
**  Configuration is probably better done using autoconf
**
**	This is the fallback version (if none other found)
**	(Actually, currently hacked to work on my development platforms)
*/

// these should be defined on all POSIX platforms
# define EP_OSCF_HAS_INTTYPES_H		1	// does <inttypes.h> exist?
# define EP_OSCF_HAS_STDBOOL_H		1	// does <stdbool.h> exist?
# define EP_OSCF_HAS_STDINT_H		1	// does <stdint.h> exist?
# define EP_OSCF_HAS_STDLIB_H		1	// does <stdlib.h> exist?
# define EP_OSCF_HAS_STRING_H		1	// does <string.h> exist?
# define EP_OSCF_HAS_SYS_TYPES_H	1	// does <sys/types.h> exist?
# define EP_OSCF_HAS_UNISTD_H		1	// does <unistd.h> exist?

# ifdef __FreeBSD__
#  define EP_OSCF_HAS_UCHAR_T		0	// does uchar_t exist?
#  define EP_OSCF_HAS_SYS_CDEFS_H	1	// does <sys/cdefs.h> exist?
#  define EP_OSCF_HAS_STRLCPY		1	// does strlcat(3) exist?
#  define EP_OSCF_HAS_LSTAT		1	// does lstat(2) exist?
#  if __FreeBSD_version >= 440000
#   define EP_OSCF_HAS_GETPROGNAME	1	// does getprogname(3) exist?
#  endif
# endif // __FreeBSD__

# ifdef __APPLE__
#  define EP_OSCF_HAS_UCHAR_T		0	// does uchar_t exist?
#  define EP_OSCF_HAS_SYS_CDEFS_H	1	// does <sys/cdefs.h> exist?
#  define EP_OSCF_HAS_STRLCPY		1	// does strlcat exist?
#  define EP_OSCF_HAS_LSTAT		1	// does lstat(2) exist?
#  define EP_OSCF_HAS_GETPROGNAME	1	// does getprogname(3) exist?
#  define EP_OSCF_SYSTEM_VALLOC		valloc	// aligned memory allocator
# endif // __APPLE__

#ifdef __linux__
# define EP_TYPE_PRINTFLIKE(a, b)
# define EP_OSCF_HAS_STRLCPY		0	// no strlcpy on linux
#endif

// this is a heuristic, but I don't know if there's a way to do it right
# if defined(__amd64) || defined(__x86_64)
#  define EP_OSCF_64BITPTR		1
# else
#  define EP_OSCF_64BITPTR		0
# endif

#ifndef EP_TYPE_PRINTFLIKE
# define EP_TYPE_PRINTFLIKE(a, b)	__printflike(a, b)
#endif

#endif // _EP_CONF_H_
