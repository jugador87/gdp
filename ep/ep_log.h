/***********************************************************************
**	Copyright (c) 2008-2014, Eric P. Allman.  All rights reserved.
**	$Id: ep_log.h 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

////////////////////////////////////////////////////////////////////////
//
//  LOGGING
//
////////////////////////////////////////////////////////////////////////

#ifndef _EP_LOG_H_
#define _EP_LOG_H_


typedef struct EP_LOG_TYPE	EP_LOG_TYPE;

// a log instance
typedef struct EP_LOG
{
	EP_OBJ_HEADER				// we are an object

	const EP_LOG_TYPE	*ltype;		// type of this log
	const char		*name;		// log name
	const char		*app;		// application name for entries
	const void		*priv;		// private data for log type
	uint32_t		flags;		// flags, see below
} EP_LOG;

// bit values for flags
#define EP_LOG_NORMAL		0x0000	// no interesting flags
#define EP_LOG_NODEFAULT	0x0001	// never make this the default


// The type of a log, mostly callbacks
//   Arguably this should be a subclass of EpClassLogType, not an instance
struct EP_LOG_TYPE
{
	EP_OBJ_HEADER			// we are an object
	EP_STAT		(*open)(EP_LOG *log);
	EP_STAT		(*close)(EP_LOG *log);
	EP_STAT		(*log)(
				EP_LOG *log,
				int sev,
				const char *msgid,
				va_list av);
};

/*
**  Log Severities
**  	These are mostly duplicates of the status code severities
*/

#define EP_LOG_SEV_INFO		EP_STAT_SEV_OK
#define EP_LOG_SEV_WARN		EP_STAT_SEV_WARN
#define EP_LOG_SEV_ERROR	EP_STAT_SEV_ERROR
#define EP_LOG_SEV_SEVERE	EP_STAT_SEV_SEVERE
#define EP_LOG_SEV_ABORT	EP_STAT_SEV_ABORT
#define EP_LOG_SEV_PANIC	(8)



extern EP_LOG	*ep_log_open(
			const EP_LOG_TYPE *ltype,	// type of this log
			const char *lname,		// name or file
			const char *tag,		// tag for entries
			uint32_t flags,			// see below
			const void *priv,		// per-type data
			EP_STAT *pstat);		// return status code
extern void	ep_log_propl(
			EP_LOG *log,			// log to write
			int sev,			// message severity
			const char *msgid,		// message code
			...);				// tag=val pairs
extern void	ep_log_propv(
			EP_LOG *log,
			int sev,
			const char *msgid,
			va_list av);
extern void	ep_log_tostr(
			FILE *logsp,		// stream to write
			int sev,			// message severity
			const char *msgid,		// message code
			va_list av);			// tag=value pairs

extern const EP_OBJ_CLASS	EpClassLog;
extern const EP_OBJ_CLASS	EpClassLogType;

// some common log types
extern const EP_LOG_TYPE	EpLogTypeSyslog;	// system default
extern const EP_LOG_TYPE	EpLogTypeStream;	// log to a stream


//////////////////////////////////////////////////////////////////////
//
//  OBSOLETE STUFF
//

#if 0
#if EP_LOG_STYLE == EP_LOG_STYLE_UNIX

# define EP_LOG_ID(str, code)		(str)
typedef char		*EP_LOG_LABEL;

#elif EP_LOG_STYLE == EP_LOG_STYLE_WIN32

# define EP_LOG_ID(str, code)		(code)
typedef WIN32CODE	EP_LOG_LABEL;

#endif // EP_LOG_STYLE
#endif // 0

//
//  END OF OBSOLETE STUFF
//
//////////////////////////////////////////////////////////////////////

#endif /*_EP_LOG_H_*/
