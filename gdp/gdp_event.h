/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_EVENT representation
**
**		These are linked onto either a free list or an active list.
**		gdp_event_next gets the next event from the active list.
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
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

#ifndef _GDP_EVENT_H_
#define _GDP_EVENT_H_

#include "gdp_priv.h"

struct gdp_event
{
	STAILQ_ENTRY(gdp_event)	queue;		// free/active queue link
	int						type;		// event type
	gdp_gcl_t				*gcl;		// GCL handle for event
	gdp_datum_t				*datum;		// datum for event
	gdp_event_cbfunc_t		cb;			// callback for event
	void					*udata;		// user data
	EP_STAT					stat;		// detailed status code
};

// set up request to deliver events or callbacks
extern void				_gdp_event_setcb(
								gdp_req_t *req,
								gdp_event_cbfunc_t cbfunc,
								void *cbarg);

// create asynchronous event based on a request
extern EP_STAT			_gdp_event_add_from_req(
								gdp_req_t *req);

#endif // _GDP_EVENT_H_
