/* vim: set ai sw=8 sts=8 :*/

/*
**  Extended Status modules
**  	Based on libep, (c) 2012 Eric P. Allman
**
**	Status codes are integers cast to a struct so that they aren't
**	type compatible.  They can return integers on success, but not
**	to the full range of the machine; in particular, they can't
**	return pointers.
**
**	The top few bits encode a severity so the caller can easily
**	determine success or failure with a bit mask.  Failures are
**	further subdivided --- for example, there is a range
**	corresponding to errno codes.
**
**	Status codes are four parts: a severity, a registry, a
**	module, and a detail.  These should be considered opaque
**	by applications.
**
**	The actual implementation (at the moment) looks as follows:
**
**	+-----+---------------------+----------+-------------+
**	| sev |      registry       |  module  |   detail    |
**	+-----+---------------------+----------+-------------+
**	 3 bit        11 bits          8 bits     10/42 bits
**
**	Severities with the top bit being zero indicate success, and
**	the rest of the word can convey arbitary information.  If the
**	top bit of the severity is one there is an error with severity:
**	    EP_STAT_SEV_WARN --- warning or temporary error that is likely
**	    	to recover by itself, e.g., connection refused.
**	    EP_STAT_SEV_ERROR --- ordinary permanent error.
**	    EP_STAT_SEV_SEVERE --- severe error that requires immediate
**	    	recovery, e.g., out of memory.
**	    EP_STAT_SEV_ABORT --- internal error detected, such
**	    	as a software bug, assertion error, etc.  Recovery
**	    	generally not possible.
**
**	The registry is global, and must be assigned through a central
**	authority.  As a special case, the "local" registry is for use
**	within a single program (as opposed to libraries).
**
**	Normally a registry would allocate different modules for
**	different libraries or applications (obviously applications
**	can overlap), and detail is for the use of that module or
**	application.
**
**	As a special case, a severity code of zero (OK) can also
**	be used to encode an integer return value of up to 2^31
**	(2^63 on 64-bit machines).
*/

#ifndef _EP_STAT_H
# define _EP_STAT_H

# include <ep/ep.h>
# include <limits.h>

typedef struct _ep_stat
{
	unsigned long	code;
} EP_STAT;

#define _EP_STAT_SEVBITS	3
#define _EP_STAT_REGBITS	11
#define _EP_STAT_MODBITS	8

#if INT_MAX == INT32_MAX
# define _EP_STAT_DETBITS	10
#else
# define _EP_STAT_DETBITS	42
#endif

#define EP_STAT_MAX_REGISTRIES	((1 << _EP_STAT_REGBITS) - 1)
#define EP_STAT_MAX_MODULES	((1 << _EP_STAT_MODBITS) - 1)
#define EP_STAT_MAX_DETAIL	((1 << _EP_STAT_DETBITS) - 1)

#define _EP_STAT_MODSHIFT	_EP_STAT_DETBITS
#define _EP_STAT_REGSHIFT	(_EP_STAT_MODSHIFT + _EP_STAT_MODBITS)
#define _EP_STAT_SEVSHIFT	(_EP_STAT_REGSHIFT + _EP_STAT_REGBITS)

#define EP_STAT_SEV_OK		(0)	// everything OK (also 1, 2, and 3)
#define EP_STAT_SEV_WARN	(4)	// warning or temp error, may work later
#define EP_STAT_SEV_ERROR	(5)	// normal error
#define EP_STAT_SEV_SEVERE	(6)	// severe error, should back out
#define EP_STAT_SEV_ABORT	(7)	// internal error

// constructors for status code
#define EP_STAT_NEW(s, r, m, d) \
			((EP_STAT) { ((((s) & ((1 << _EP_STAT_SEVBITS) - 1)) << _EP_STAT_SEVSHIFT) | \
				      (((r) & ((1 << _EP_STAT_REGBITS) - 1)) << _EP_STAT_REGSHIFT) | \
				      (((m) & ((1 << _EP_STAT_MODBITS) - 1)) << _EP_STAT_MODSHIFT) | \
				      (((d) & ((1 << _EP_STAT_DETBITS) - 1)))) } )

// routines to extract pieces of error codes
#define EP_STAT_SEVERITY(c)	(((c).code >> _EP_STAT_SEVSHIFT) & ((1 << _EP_STAT_SEVBITS) - 1))
#define EP_STAT_REGISTRY(c)	(((c).code >> _EP_STAT_REGSHIFT) & ((1 << _EP_STAT_REGBITS) - 1))
#define EP_STAT_MODULE(c)	(((c).code >> _EP_STAT_MODSHIFT) & ((1 << _EP_STAT_MODBITS) - 1))
#define EP_STAT_DETAIL(c)	(((c).code                     ) & ((1 << _EP_STAT_SEVBITS) - 1))

// predicates to query the status severity
#define EP_STAT_ISOK(c)		(EP_STAT_SEVERITY(c) < EP_STAT_SEV_WARN)
#define EP_STAT_ISWARN(c)	(EP_STAT_SEVERITY(c) == EP_STAT_SEV_WARN)
#define EP_STAT_ISERROR(c)	(EP_STAT_SEVERITY(c) == EP_STAT_SEV_ERROR)
#define EP_STAT_ISSEVERE(c)	(EP_STAT_SEVERITY(c) == EP_STAT_SEV_SEVERE)
#define EP_STAT_ISFAIL(c)	(EP_STAT_SEVERITY(c) >= EP_STAT_SEV_WARN)
#define EP_STAT_ISPFAIL(c)	(EP_STAT_SEVERITY(c) >= EP_STAT_SEV_ERROR)

// compare two status codes for equality
#define EP_STAT_IS_SAME(a, b)	((a).code == (b).code)

// casting to and from int
#define EP_STAT_TO_INT(s)	((s).code)
#define EP_STAT_FROM_INT(i)	((EP_STAT) { (i) })

// error checking quick routine, e.g., EP_STAT_CHECK(stat, break);
#define EP_STAT_CHECK(st, failure) \
		{ \
			if (!EP_STAT_ISOK(st)) \
				{ failure; } \
		}

// functions
extern EP_STAT	ep_stat_from_errno(int uerrno);
extern char	*ep_stat_tostr(EP_STAT, char *, size_t);

/**********************************************************************
**
**  Registries
**  	Should probably be in a separate header file
*/

#define EP_STAT_REG_LOCAL	0x03FE	// for local use
#define EP_STAT_REG_EP		0x1F00	// libep internal use

/**********************************************************************
**
**  Modules specific to EP_STAT_REG_EP
*/

#define EP_STAT_MOD_GENERIC	0	// basic multi-use errors
#define EP_STAT_MOD_ERRNO	0x0FE	// corresponds to errno codes

// common status code definitions
#define _EP_STAT_INTERNAL(sev, mod, code) \
		EP_STAT_NEW(EP_STAT_SEV_ ## sev, EP_STAT_REG_EP, mod, code)

// generic status codes
#define EP_STAT_OK		EP_STAT_NEW(EP_STAT_SEV_OK, 0, 0, 0)
#define EP_STAT_WARN		_EP_STAT_INTERNAL(WARN, EP_STAT_MOD_GENERIC, 0)
#define EP_STAT_ERROR		_EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_GENERIC, 0)
#define EP_STAT_SEVERE		_EP_STAT_INTERNAL(SEVERE, EP_STAT_MOD_GENERIC, 0)
#define EP_STAT_ABORT		_EP_STAT_INTERNAL(ABORT, EP_STAT_MOD_GENERIC, 0)

// common shared errors
#define EP_STAT_OUT_OF_MEMORY	_EP_STAT_INTERNAL(SEVERE, EP_STAT_MOD_GENERIC, 1)
#define EP_STAT_ARG_OUT_OF_RANGE _EP_STAT_INTERNAL(ERROR, EP_STAT_MOD_GENERIC, 2)
#define EP_STAT_END_OF_FILE	_EP_STAT_INTERNAL(WARN, EP_STAT_MOD_GENERIC, 3)


// handle on status handler -- to release later
typedef struct EP_STAT_HANDLE	EP_STAT_HANDLE;

// status handling function
typedef EP_STAT	(*EP_STAT_HANDLER_FUNCP)
			(EP_STAT estat,		// status code
			//char *module,		// calling module
			const char *defmsg,	// default message
			va_list av);		// string arguments

// return string representation of status
char		*ep_stat_tostr(
			EP_STAT estat,
			char *buf,
			size_t bsize);
// return string representation of severity (in natural language)
const char	*ep_stat_sev_tostr(
			int sev);
// register/deregister a status handler
EP_STAT_HANDLE	*ep_stat_register(
			EP_STAT estat,
			EP_STAT mask,
			EP_STAT_HANDLER_FUNCP f);
EP_STAT		ep_stat_unregister(
			EP_STAT_HANDLE *h);
// post a status
EP_STAT		ep_stat_post(
			EP_STAT c,
			const char *defmsg,
			...);
EP_STAT		ep_stat_vpost(
			EP_STAT c,
			const char *defmsg,
			va_list av);
// print a status
void		ep_stat_print(
			EP_STAT c,
			const char *defmsg,
			FILE *fp, ...);
void		ep_stat_vprint(
			EP_STAT c,
			const char *defmsg,
			FILE *fp,
			va_list av);
// print and abort (never returns)
void		ep_stat_abort(
			EP_STAT c);
#endif // _EP_STAT_H
