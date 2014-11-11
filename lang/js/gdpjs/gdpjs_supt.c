/* vim: set ai sw=4 sts=4 ts=4 : */

// Support routines for Node.js Javascript programs accessing the
// GDP dynamic libraries, libgdp.xxx and libep.xxx as well as some
// GDP macros, and system (e.g., libc) routines.
//
// See libgdp_h.js for Node.js ffi "declarations" for these C routines
// to allow invoking them from JS.
//
// Alec Dara-Abrams
//  2014-10-28


#include "gdpjs_supt.h"
#include "gdp/gdp.h"
#include "ep/ep.h"
#include "ep/ep_stat.h"
// Seems to be a problem defining EP_STAT if ep/ep.h is not included before
// ep/ep_stat.h .  Check includes in ep_stat.h .


// DEBUG = 1 to include fprintf(stderr, ) debugging output; else = 0.
#define DEBUG 0


// EP_STAT_INT_EQUIV is needed for some casts below.
// We determine this by examning EP_STAT in ep/ep_stat.h .
// We assume these BIT counts exhaust EP_STAT.
#define EP_STAT_NBITS  ( _EP_STAT_SEVBITS + _EP_STAT_REGBITS + \
                         _EP_STAT_MODBITS + _EP_STAT_DETBITS )

#if   (EP_STAT_NBITS ==  32)
	#define EP_STAT_INT_EQUIV  uint32_t
#elif (EP_STAT_NBITS ==  64)
	#define EP_STAT_INT_EQUIV  uint64_t
#else
	#error "The type EP_STAT (in ep/ep_stat.h) must be 32 or 64 bits long"
#endif

// EP_STAT_32_64 is needed for some casts below
// Note, even though EP_STAT is all caps, it is a C typedef struct, not a
// cpp macro.  Yes, we may be making some assumptions here about padding, etc.
// but we only use this type for casts, and don't access internal structure.
typedef union
		{ EP_STAT            as_EP_STAT;
	      EP_STAT_INT_EQUIV  as_32_64_t;
        } EP_STAT_32_64;



// Some general libc-related functions

// Forces a flush() on stdout, on stderr, and on all open file descriptors.
// Node.js console.log(), console.error(), and process.stdxxx.write() are,
// evidently, buffered when writing to a file.  Output to a terminal seems
// to be unbuffered.
int fflush_stdout() { return ( fflush(stdout) ); }
int fflush_stderr() { return ( fflush(stderr) ); }
int fflush_all()    { return ( fflush( NULL ) ); }



// Some general libgdp/libep-related functions


// Returns the size in bytes of the libep error status code, EP_STAT .
size_t
sizeof_EP_STAT_in_bytes()
{
#if DEBUG
    fprintf( stderr,
	         "sizeof_EP_STAT_in_bytes: sizeof(EP_STAT) = %lu (decimal-%%lu)\n",
	         sizeof(EP_STAT)
		   );
#endif
	return (sizeof(EP_STAT));
}


// ====================================================
// Wrap selected functions from gdp/

// From gdp/gdp.h

// We need these specialized ..._print_stdout() wrappers because Node.js JS
// cannot supply a true stdout FILE* for calls to the node-ffi FFI.

// print a GCL (for debugging)
// extern void
// gdp_gcl_print(
//     const gdp_gcl_t *gclh,  // GCL handle to print
//     FILE *fp,               // file to print it to
//     int detail,             // not used at this time
//     int indent              // not used at this time
// );

// Forwards to gcp_gcl_print( const gdp_gcl_t *gclh, stdout, , , );
void
gdp_gcl_print_stdout(
    const gdp_gcl_t *gclh,  // GCL handle to print
    int detail,             // not used at this time
    int indent              // not used at this time
)
{
    gdp_gcl_print( gclh, stdout, detail, indent);
}


// print a message (for debugging)
// extern void
// gdp_datum_print(
//     const gdp_datum_t *datum, // message to print
//     FILE *fp                  // file to print it to
// );

// Forwards to gdp_datum_print( const gdp_datum_t *datum, stdout );
void
gdp_datum_print_stdout(
    const gdp_datum_t *datum // message to print
)
{
    gdp_datum_print( datum, stdout );
}


// ====================================================
// Wrap selected macros from gdp/gdp*.h in functions

// From gdp/gdp_stat.h

// #define GDP_STAT_NAK_NOTFOUND GDP_STAT_NEW(ERROR, GDP_COAP_NOTFOUND)

// Returns a EP_STAT with severity field ERROR; other fields see below.
EP_STAT
gdp_stat_nak_notfound()
{
	EP_STAT_32_64 rv;
    rv.as_EP_STAT  = (GDP_STAT_NEW(ERROR, GDP_COAP_NOTFOUND));
#if DEBUG
    fprintf(stderr, "gdp_stat_nak_notfound:"
                    "  Returning GDP_STAT_NAK_NOTFOUND = %x\n", rv.as_32_64_t);
    fflush(stderr);
#endif
    return (rv.as_EP_STAT);
}


// ====================================================
// Wrap selected macros from ep/ep*.h in functions

// From ep/ep_statcodes.h

// generic status codes
// #define EP_STAT_OK            EP_STAT_NEW(EP_STAT_SEV_OK, 0, 0, 0)

// Returns a EP_STAT with severity field OK; other fields are 0.
EP_STAT
ep_stat_ok()
{
	EP_STAT_32_64 rv;
    rv.as_EP_STAT = (EP_STAT_NEW(EP_STAT_SEV_OK, 0, 0, 0));
#if DEBUG
    fprintf(stderr, "ep_stat_ok:  Returning EP_STAT_OK = %x\n", rv.as_32_64_t);
    fflush(stderr);
#endif
    return (rv.as_EP_STAT);
}


// From ep/ep_statcodes.h

// common shared errors
// #define EP_STAT_END_OF_FILE   _EP_STAT_INTERNAL(WARN, EP_STAT_MOD_GENERIC, 3)

// Returns a EP_STAT with severity field WARN; other fields see below.
EP_STAT
ep_stat_end_of_file()
{
	EP_STAT_32_64 rv;
    rv.as_EP_STAT = ( _EP_STAT_INTERNAL(WARN, EP_STAT_MOD_GENERIC, 3) );
#if DEBUG
    fprintf(stderr, "ep_stat_end_of_file:"
                    "  Returning EP_STAT_END_OF_FILE = %x\n", rv.as_32_64_t);
    fflush(stderr);
#endif
    return (rv.as_EP_STAT);
}


// From ep/ep_stat.h

// predicates to query the status severity
// #define EP_STAT_ISOK(c)       (EP_STAT_SEVERITY(c) < EP_STAT_SEV_WARN)

// Return true iff the status severity field inside ep_stat is less
// than EP_STAT_SEV_WARN.
int /* Boolean */
ep_stat_isok(EP_STAT ep_stat) {
    int rv = EP_STAT_ISOK(ep_stat);
#if DEBUG
    fprintf(stderr, "ep_stat_isok: ep_stat = %x,",
	        ((EP_STAT_32_64) ep_stat).as_32_64_t );
    fprintf(stderr, "  Returning EP_STAT_ISOK() = %x\n", rv);
    fflush(stderr);
#endif
    return rv;
}


// From ep/ep_stat.h

// compare two status codes for equality
// #define EP_STAT_IS_SAME(a, b)   ((a).code == (b).code)

// Return true iff the status codes are the same
int /* Boolean */
ep_stat_is_same(EP_STAT a, EP_STAT b) {
    int rv = EP_STAT_IS_SAME(a, b);
#if DEBUG
    fprintf(stderr, "ep_stat_is_same: a = %x, b = %x",
	        (((EP_STAT_32_64) a).as_32_64_t ),
	        (((EP_STAT_32_64) b).as_32_64_t )
		   );
    fprintf(stderr, "  Returning EP_STAT_IS_SAME() = %x\n", rv);
    fflush(stderr);
#endif
    return rv;
}

