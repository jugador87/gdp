/*
 *  GDPD_ROUTER.H --- Distributed Object Location and Routing (DOLR)
 *
 *      Based on Tapestry
 *
 *      XXX make some functions into macros?
 */


#include <gdpd/local_info.h>
#include <gdpd/gdpd_router.h>


/*********************** PRIVATE **************************/

/* gcl_name_t manipulation */

// return the HOPNO'th hop digit when routing toward OBJ
// XXX estat? maybe. or could check hopno validity before this is called
//      warning: currently assumes ROUTER_HOP_RADIX == 4
//      warning: if ROUTER_HOP_SIZE > 8, must change return type
// TODO: test
static uint8_t
router_get_hop_digit(gcl_name_t obj, uint8_t hopno)
{
    uint8_t ret = gcl_name_t[hopno >> 1];
    if ((hopno & 1) == 0)
    {
        ret >>= 4;    // assume ROUTER_HOP_RADIX == 4
    }
    
    ret &= ROUTER_HOP_MASK;
    return ret;
}

// set the HOPNO'th hop digit to DIG when routing toward OBJ
// XXX estat? maybe. or could check hopno validity before this is called
//      warning: currently assumes ROUTER_HOP_RADIX == 4
//      warning: if ROUTER_HOP_SIZE > 8, must change return and param types
// TODO: test
static void
router_set_hop_digit(gcl_name_t obj, uint8_t hopno, uint8_t dig)
{
    int idx = hopno >> 1;
    uint8_t copy = gcl_name_t[idx];
    uint8_t dig_mask;

    // sanitize digit
    if ((hopno & 1) == 0)
    {
        // assume ROUTER_HOP_RADIX == 4
        dig_mask = ROUTER_HOP_MASK << 4;    
        dig <<= 4;
    } else {
        dig_mask = ROUTER_HOP_MASK;
    }
    dig &= dig_mask;
    
    // update COPY
    copy &= ~dig_mask;
    copy |= dig;

    // use copy to update OBJ
    obj[idx] = copy;
}


/* object location cache
 *
 *      TODO support time expirations on cache entries?
 *
 *      XXX a good way to implement this?
 *
 *      XXX alternate way to implement this. EP's hash table
 *      overwrites on collisions, so maybe hash table entries will
 *      include original GCL_NAME_T so that we can compare it to
 *      the requested OBJ.
 */

typedef struct
{
    gcl_name_t name;    // object GUID
    gcl_name_t loc;     // location nodeID
} router_obj_loc_cache_entry_t;

// object location cache.
// this declaration initializes them to zero (NULL).
static router_obj_loc_cache_entry_t *router_obj_loc_cache[INT32_MAX];

// get the obj_loc_cache index to be used for the given gcl_name_t.
// Uses the first 32 bits of the name as the index.
// NOTE: for randomly generated gcl_name_t's, should be randomly distributed
// in cache.
static uint32_t router_obj_loc_cache_idx(gcl_name_t obj)
{
    uint32_t pidx = (uint32_t *) obj;
    return *idx;
}

// initialize object location cache
// XXX estat?
static void
router_init_obj_loc_cache(void)
{
    //XXX do anything here at all? Is there a point to this function?
    return;
}

// Search the cache for the node that holds the object,
// and put it in NODE.
// If no info available, NODE will be zero.
static void
router_obj_loc_cache_get(gcl_name_t obj, gcl_name_t node)
{
    uint32_t idx = router_obj_loc_cache_idx(obj);
    router_obj_loc_cache_entry_t *pentry = router_obj_loc_cache[idx];

    if (pentry == NULL) {
        memset(node, 0, sizeof gcl_name_t);
    }
}

// cache an object and its location.
// XXX epstat
//      especially add error handling for mallocs
static void
router_obj_loc_cache_set(gcl_name_t obj, gcl_name_t node)
{
    size_t namesz = sizeof gcl_name_t;

    // allocate and initialize new entry
    router_obj_loc_cache_entry_t *pe = ep_mem_malloc(sizeof *pi);

    pe->name = ep_mem_malloc(namesz);
    memcpy(pe->name, obj, namesz);

    pe->loc = ep_mem_malloc(namesz);
    mempy(pe->loc, node, namesz);

    // if we're about to replace an entry, free the old one
    uint32_t idx = router_obj_loc_cache_idx(obj);
    router_obj_loc_cache_entry_t *pe_old = router_obj_loc_cache[idx];
    if (pe_old != NULL) {
        ep_mem_free(pe_old->name);
        ep_mem_free(pe_old->loc);
        ep_mem_free(pe_old);
    }

    // insert entry
    router_obj_loc_cache[idx] = pe;
}


/* neighbor links 
 *
 *      NOTE: hops start at left size of GUID and grow right.
 */

// Neighbor links
// this declaration initializes them to zero (NULL).
// indices:
//      first:  hopno of current hop.
//      second: the digit for the next hop
//      third:  which link to use. 0 is primary, rest are backups.
static sockaddr_xx *router_nlinks[ROUTER_MAXHOPS][ROUTER_HOP_RADIX][ROUTER_NLINKS]

// initialize neighbor links
// XXX estat? probably
static void
router_init_nlinks(void)
{
    // TODO eventually, fill nlinks with the algorithms

    // NOTE: declaration of router_nlinks automagically sets pointers to NULL.
    return;
}

// return HOPNO'th hop digit when routing to OBJ.
// XXX estat? probably
//      warning: if ROUTER_HOP_SIZE > 8, must change return type
// TODO: actually use backup links
static uint8_t
router_next_hop_digit(gcl_name_t obj, uint8_t hopno)
{
    uint8_t hop_digit = router_get_hop_digit(obj, hopno);
    sockaddr_xx *psa = router_nlinks[hopno][hop_digit][0];
    if (psa != NULL) {
        return hop_digit;
    }

    while (psa == NULL) {
        hop_digit++;
        psa = router_nlinks[hopno][hop_digit][0];
    }

    return hop_digit
}


/* routing */

// Test if the current node is the object OBJ's root
// by comparing the "unrouted" parts of the identifiers.
// TODO test
static bool
router_we_are_root(gcl_name_t obj, uint8_t hopno)
{
    for ( ; hopno < ROUTER_MAXHOPS; hopno++) {
        if (router_get_hop_digit(obj, hopno) !=
            router_get_hop_digit(local_node_name, hopno))
        {
            return false
        }
    }

    return true;
}

// find the next hop to route to on the way to OBJ
// if this is the HOPNO'th hop (indexed from 0).
// HOPNO will be updated to reflect the next hop,
// and the next node will be placed in NEXT.
// TODO test
static void
router_next_hop(gcl_name_t obj, uint8_t *phopno, gcl_name_t next)
{
    memcpy(next, obj, sizeof gcl_name_t);

    if (ROUTER_DONE_HOPPING(hopno))
    {
        // no more hops, we're the root.
        return;
    }

    uint8_t next_hop_digit = router_next_hop_digit(obj, *phopno);
    router_set_hop_digit(next, *phopno, next_hop_digit);
    *phopno += 1;

    // if we're the next node
    // TODO relook this over
    if (router_get_hop_digit(obj, *phopno) ==
        router_get_hop_digit(local_node_name, *phopno))
    {
        router_next_hop(obj, phopno, next); // (changes NEXT and HOPNO)
    }
}


/*********************** PUBLIC ***************************/

// initialize the router
// XXX should return estat?
void
router_init(void)
{
    // initialize object location cache
    router_init_obj_loc_cache();
    
    // initialize routing table
    router_init_nlinks();
}

// determine which node holds the requested object
EP_STAT
router_find_obj(struct bufferevent *bev, conn_t *c,
    gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    // TODO

    // request packet data format:
    //      (1)     hopno       hop number, from 0
    //      (4)     src_ip      request source ip(4)    XXX later, support ipv6
    //      (2)     src_port    request source port

    uint8_t *dp;
    uint8_t dbuf[64];   // XXX more than enough
    int i;
    EP_STAT estat;

    uint8_t hopno;
    gcl_name_t cached_loc;

    if (cpkt->dlen > sizeof dbuf)
        dp = ep_mem_malloc(cpkt->dlen);
    else
        dp = dbuf;

    // read in the data from the bufferevent
    i = evbuffer_remove(bufferevent_get_input(bev), dp, cpkt->dlen);
    if (i < cpkt->dlen)
    {
        return gdpd_gcl_error(cpkt->gcl_name, "router_find_obj: short read",
                GDP_STAT_SHORTMSG, GDP_NAK_S_INTERNAL);
    }

    hopno = dp[0];
    router_cache_get_obj_loc(cpkt->gcl_name, cached_obj_loc);
    
    // if we're the object's root, or if we've cached its location:
    if (!gdp_gcl_name_is_zero(cached_obj_loc) ||
        router_we_are_root(cpkt->gcl_name, hopno))
    {
        // read in rest of data
        sockaddr_in origin;
        origin.sin_family = AF_INET;
        origin.sin_addr = *((uint32_t *)(dp+1));
        origin.sin_port = *((uint16_t *)(dp+5));

        // TODO
        // tell original requester where the object is.
        // I've decided to use the IP address, and not worry about
        // the node moving for now and changing ip addresses.
        //
        // if we got the request from the original requester
        // (meaning the request was never forwarded), maybe we can
        // just use the existing socket connection and not have to
        // open a new one.

    } else {
        // find next node to hop to
        gcl_name_t next_node;
        router_next_hop(cpkt->gcl_name, &hopno, next_node);
        dp[0] = hopno;

        // forward request to next node
        // TODO
    }

    if (dp != dbuf)
	ep_mem_free(dp);
}
