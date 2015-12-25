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

#ifndef _GDPLOGD_H_
#define _GDPLOGD_H_		1

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
**  Private GCL definitions for gdpd only
**
**		The gcl field is because the LIST macros don't understand
**		having the links in a substructure (i.e., I can't link a
**		gdp_gcl_xtra to a gdp_gcl).
*/

struct gdp_gcl_xtra
{
	// declarations relating to semantics
	gdp_gcl_t			*gcl;			// enclosing GCL

	// physical implementation declarations
	long				ver;			// version number of on-disk file
	FILE				*fp;			// pointer to the on-disk file
	struct log_index	*log_index;		// recno -> offset index
	off_t				data_offset;	// offset for start of data
	uint16_t			nmetadata;		// number of metadata entries
	uint16_t			log_type;		// from log header
	gdp_recno_t			recno_offset;	// recno offset (first stored recno - 1)
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

#endif //_GDPLOGD_H_
