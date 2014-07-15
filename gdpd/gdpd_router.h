/*
 *  GDPD_ROUTER.H --- Distributed Object Location and Routing (DOLR)
 *
 *      Based on Tapestry
 */

#ifndef _GDPD_ROUTER_H_
#define _GDPD_ROUTER_H_


#include <gdp/gdp.h>
#include <gdp/gdp_protocol.h>
#include <ep/ep.h>
#include <event2/bufferevent.h>


// number of bits in an identifier (GUID, nodeID)
#define ROUTER_IDSIZE ((sizeof (gcl_name_t)) << 3)

/* Global Routing */

// number of bits consumed on each hop
// XXX warning: consult gdpd_router.c before changing this value
#define ROUTER_HOP_SIZE 4
// mask to identify hop digit
#define ROUTER_HOP_MASK ((1 << ROUTER_HOP_SIZE) - 1)
// radix corresponding to the hop size
#define ROUTER_HOP_RADIX (1 << ROUTER_HOP_SIZE)

// maximum number of hops that might be made during Plaxton Routing.
// also the hop number after all possible hops have been made,
// due to 0-indexing of hop numbers.
#define ROUTER_MAXHOPS (ROUTER_IDSIZE / ROUTER_HOP_SIZE)

// whether or not we're done hopping
#define ROUTER_DONE_HOPPING(hopno)  ((hopno) >= ROUTER_MAXHOPS)

// number of links to store in each neighbor link slot.
// If >1, provides reduncancy in case the neighbor is down.
#define ROUTER_NLINKS 1     // 1: no reduncancy


// XXX will this work? Idk.
#ifndef conn_t
# define conn_t
#endif


// determine which node holds the requested object
EP_STAT
router_find_obj(
        struct bufferevent *bev,
        conn_t *c,
        gdp_pkt_hdr_t *cpkt,
        gdp_pkt_hdr_t *rpkt);

#endif //_GDPD_ROUTER_H_
