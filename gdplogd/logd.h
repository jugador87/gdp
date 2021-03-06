/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	Headers for the GDP Log Daemon
**
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**	----- END LICENSE BLOCK -----
*/

#ifndef _GDPLOGD_LOGD_H_
#define _GDPLOGD_LOGD_H_		1

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_stat.h>

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <gdp/gdp_pdu.h>
#include <gdp/gdp_stat.h>

#include <unistd.h>
#include <string.h>
#include <sys/queue.h>


// how strongly we enforce signatures
uint32_t	GdpSignatureStrictness;		// how strongly we enforce signatures

#define GDP_SIG_MUSTVERIFY	0x01		// sig must verify if it exists
#define GDP_SIG_REQUIRED	0x02		// sig must exist if pub key exists
#define GDP_SIG_PUBKEYREQ	0x04		// public key must exist

/*
**  Private GCL definitions for gdplogd only
**
**		The gcl field is because the LIST macros don't understand
**		having the links in a substructure (i.e., I can't link a
**		gdp_gcl_xtra to a gdp_gcl).
*/

typedef struct physinfo	gcl_physinfo_t;
struct gdp_gcl_xtra
{
	// declarations relating to semantics
	gdp_gcl_t				*gcl;			// enclosing GCL
	uint16_t				n_md_entries;	// number of metadata entries
	uint16_t				log_type;		// from log header

	// physical implementation declarations
	struct gcl_phys_impl	*physimpl;		// physical implementation
	gcl_physinfo_t			*physinfo;		// info needed by physical module
};


/*
**  Definitions for the gdpd-specific GCL handling
*/

extern EP_STAT	gcl_alloc(				// allocate a new GCL
					gdp_name_t gcl_name,
					gdp_iomode_t iomode,
					gdp_gcl_t **pgcl);

extern EP_STAT	gcl_open(				// open an existing physical GCL
					gdp_name_t gcl_name,
					gdp_iomode_t iomode,
					gdp_gcl_t **pgcl);

extern void		gcl_close(				// close an open GCL
					gdp_gcl_t *gcl);

extern void		gcl_touch(				// make a GCL recently used
					gdp_gcl_t *gcl);

extern void		gcl_showusage(			// show GCL LRU list
					FILE *fp);

extern EP_STAT	get_open_handle(		// get open handle (pref from cache)
					gdp_req_t *req,
					gdp_iomode_t iomode);

extern void		gcl_reclaim_resources(void);	// reclaim old GCLs


/*
**  Definitions for the protocol module
*/

extern EP_STAT	gdpd_proto_init(void);	// initialize protocol module

extern EP_STAT	dispatch_cmd(			// dispatch a request
					gdp_req_t *req);


/*
**  Advertisements
*/

extern EP_STAT	logd_advertise_all(int cmd);

extern void		logd_advertise_one(gdp_name_t name, int cmd);

extern void		sub_send_message_notification(
					gdp_req_t *req,
					gdp_datum_t *datum,
					int cmd);

/*
**  Physical Implementation --- these are the routines that implement the
**			on-disk (or in-memory) structure.
*/

struct gcl_phys_impl
{
	EP_STAT		(*init)(void);
	EP_STAT		(*read)(
						gdp_gcl_t *gcl,
						gdp_datum_t *datum);
	EP_STAT		(*create)(
						gdp_gcl_t *pgcl,
						gdp_gclmd_t *gmd);
	EP_STAT		(*open)(
						gdp_gcl_t *gcl);
	EP_STAT		(*close)(
						gdp_gcl_t *gcl);
	EP_STAT		(*append)(
						gdp_gcl_t *gcl,
						gdp_datum_t *datum);
	EP_STAT		(*getmetadata)(
						gdp_gcl_t *gcl,
						gdp_gclmd_t **gmdp);
	EP_STAT		(*newextent)(
						gdp_gcl_t *gcl);
	void		(*foreach)(
						void (*func)(
							gdp_name_t name,
							void *ctx),
						void *ctx);
};

// known implementations
extern struct gcl_phys_impl		GdpDiskImpl;

#endif //_GDPLOG_LOGD_H_
