/* vim: set ai sw=4 sts=4 :*/
/*
**  GDP_NEXUS.H --- public headers for use of the Swarm Data Plane nexus
*/

#ifndef _GDP_NEXUS_H_
#define _GDP_NEXUS_H_

#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>
#include <uuid/uuid.h>
#include <ep/ep_stat.h>
#include <event2/event.h>
#include <gdp/gdp_timestamp.h>

/**********************************************************************
**  Opaque structures
*/

// XXX not clear if we need this publicly exposed (or even what's in it)
typedef struct nexus_t	nexus_t;

// an open handle on a nexus (opaque)
typedef struct nexdle_t	nexdle_t;

// the internal name of a nexus
typedef uuid_t		nname_t;

// the printable name of a nexus
#if __linux__ || __FreeBSD__
typedef char		nexus_pname_t[37];
#else
typedef uuid_string_t	nexus_pname_t;
#endif

typedef enum
{
    GDP_MODE_RO,	// read only
    GDP_MODE_AO,	// append only
} gdpiomode_t;


/**********************************************************************
**  Geographic locations
**
**	XXX: I haven't really thought this through yet.
**	XXX: Should locations also have a confidence interval?
**	XXX: In any case, this only works for external addresses.
*/

typedef struct
{
    double	lat;	    // latitude in degrees
    double	lon;	    // longitude in degrees
    double	elev;	    // elevation in meters from mean sea level
} geoloc_t;

/**********************************************************************
**   Messages
**	These are the underlying unit that is passed through a nexus.
**
**	XXX: Is the timestamp the commit timestamp into the dataplane
**	XXX: or the timestamp of the sample itself (if known)?  Do we
**	XXX: need two timestamps?  The sample timestamp is really an
**	XXX: application level concept, so arguably doesn't belong here.
**	XXX: But that's true of the location as well.
*/

typedef struct
{
    bool	    ts_valid:1;	    // set if ts is valid
    bool	    loc_valid:1;    // set if loc is valid
    tt_interval_t   ts;		    // timestamp for this message
    geoloc_t	    loc;	    // location of this sample
    long	    msgno;	    // the message number
    const char	    *data;	    // pointer to data
    size_t	    len;	    // length of data
    // from here on down, private data
    long	    offset;	    // physical offset into media
} nexmsg_t;

/**********************************************************************
**  Public globals and functions
*/

struct event_base	*GdpEventBase;	// the base for all GDP events

// initialize the library
extern EP_STAT	gdp_init(void);

// create a new nexus
extern EP_STAT	gdp_nexus_create(
		nexus_t *,		// type information (unused)
		nexdle_t **);		// pointer to result nexdle

// open an existing network
extern EP_STAT	gdp_nexus_open(
		nname_t name,		// nexus name to open
		gdpiomode_t rw,		// read/write (append)
		nexdle_t **nexdle);	// pointer to result nexdle

// close an open nexus
extern EP_STAT	gdp_nexus_close(
		nexdle_t *nexdle);	// nexus handle to close

// create a new nexus message
extern EP_STAT	gdp_nexus_msg_new(
		nexdle_t *nexdle,
		nexmsg_t **);		// result area for message

// append to a writable nexus
extern EP_STAT	gdp_nexus_append(
		nexdle_t *nexdle,	// writable nexus handle
		nexmsg_t *);		// message to write

// read from a readable nexus
extern EP_STAT	gdp_nexus_read(
		nexdle_t *nexdle,	// readable nexus handle
		long msgno,		// offset into nexus (msg number)
		nexmsg_t *msg,		// pointer to result message
		char *buf,		// buffer space for msg
		size_t buflen);		// length of buf

// subscribe to a readable nexus
typedef void	(*gdp_nexus_sub_cbfunc_t)(  // the callback function
		nexdle_t *nexdle,	// the nexdle triggering the call
		nexmsg_t *msg,		// the message triggering the call
		void *cbarg);		// an arbitrary argument

extern EP_STAT	gdp_nexus_subscribe(
		nexdle_t *nexdle,	// readable nexus handle
		gdp_nexus_sub_cbfunc_t	cbfunc,
					// callback function for next msg
		long off,		// starting offset
		void *buf,		// buffer space for msg
		size_t buflen,		// length of buf
		void *cbarg);		// argument passed to callback

// return the name of a nexus
//	XXX: should this be in a more generic "getstat" function?
extern const nname_t	*gdp_nexus_getname(
		const nexdle_t *nexdle);	// open nexus handle

// print a nexus (for debugging)
extern EP_STAT gdp_nexus_print(
		const nexdle_t *nexdle,	// nexdle to print
		FILE *fp,		// file to print it to
		int detail,		// not used at this time
		int indent);		// not used at this time

// print a nexus message (for debugging)
extern void	gdp_nexus_msg_print(
		const nexmsg_t *msg,	// message to print
		FILE *fp);		// file to print it to

/*
**  GDP_NEXUS_PRINTABLE_NAME --- get printable name for a nexus
**  GDP_NEXUS_INTERNAL_NAME --- turn a printable name into an internal name
*/

#define gdp_nexus_printable_name(internal, external)  \
		uuid_unparse(internal, external)
#define gdp_nexus_internal_name(external, internal) \
		uuid_parse(external, internal)

/*
**  GDP-specific status codes
**
**	These should actually be separate codes, not overloads
*/

#define GDP_STAT_READ_OVERFLOW	    EP_STAT_END_OF_FILE
#define GDP_STAT_NOT_IMPLEMENTED    EP_STAT_SEVERE

#endif // _GDP_NEXUS_H_
