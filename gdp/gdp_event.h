/* vim: set ai sw=4 sts=4 ts=4 :*/

#ifndef _GDP_EVENT_H_
#define _GDP_EVENT_H_

/*
**	GDP_EVENT representation
**
**		These are linked onto either a free list or an active list.
**		gdp_event_next gets the next event from the active list.
*/

struct gdp_event
{
	gdp_event_t		*next;		// link to next event
	int				type;		// event type
	gdp_gcl_t		*gcl;		// GCL handle for event
	gdp_datum_t		*datum;		// datum for event
};

// allocate an event
extern EP_STAT			gdp_event_new(gdp_event_t **gevp);

// add an event to the active queue
extern void				gdp_event_trigger(gdp_event_t *gev);

#endif // _GDP_EVENT_H_
