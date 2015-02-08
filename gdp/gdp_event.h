/* vim: set ai sw=4 sts=4 ts=4 :*/

#ifndef _GDP_EVENT_H_
#define _GDP_EVENT_H_

#include "gdp_priv.h"

/*
**	GDP_EVENT representation
**
**		These are linked onto either a free list or an active list.
**		gdp_event_next gets the next event from the active list.
*/

struct gdp_event
{
	STAILQ_ENTRY(gdp_event)	queue;		// free/active queue link
	int						type;		// event type
	gdp_gcl_t				*gcl;		// GCL handle for event
	gdp_datum_t				*datum;		// datum for event
	gdp_gcl_sub_cbfunc_t	cb;			// callback for event
	void					*udata;		// user data
};

// allocate an event
extern EP_STAT			_gdp_event_new(gdp_event_t **gevp);

// add an event to the active queue
extern void				_gdp_event_trigger(gdp_event_t *gev);

// start up an event callback thread (if not already running)
extern EP_STAT			_gdp_event_start_cb_thread(void);

#endif // _GDP_EVENT_H_
