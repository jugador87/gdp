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
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
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

// allocate an event
extern EP_STAT			_gdp_event_new(gdp_event_t **gevp);

// add an event to the active queue
extern void				_gdp_event_trigger(gdp_event_t *gev);

// set up request to deliver events or callbacks
extern void				_gdp_event_setcb(
								gdp_req_t *req,
								gdp_event_cbfunc_t cbfunc,
								void *cbarg);

#endif // _GDP_EVENT_H_
