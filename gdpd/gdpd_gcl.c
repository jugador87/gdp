/* vim: set ai sw=4 sts=4 ts=4 : */

#include "gdpd.h"
#include "gdpd_gcl.h"
#include "gdpd_physlog.h"

#include <event2/event.h>
#include <event2/buffer.h>

#include <errno.h>

/*
**	GCL_NEW --- create a new GCL Handle & initialize
**
**		This is almost identical to gdp_gclh_new in the GDP library,
**		but initializes the extra fields in the GCL Handle in the
**		daemon only (specifically the offset cache).  Other than
**		that, this routine needs to be compatible with it's
**		equivalent in the GDP library.
**
**	Returns:
**		New GCL Handle if possible
**		NULL otherwise
*/

EP_STAT
gdpd_gclh_new(gcl_handle_t **pgclh)
{
	EP_STAT estat = EP_STAT_OK;
	gcl_handle_t *gclh;

	// allocate the memory to hold the GCL Handle
	gclh = ep_mem_zalloc(sizeof *gclh);
	if (gclh == NULL)
		goto fail1;

	//estat = gcl_offset_cache_init(gclh);
	//EP_STAT_CHECK(estat, goto fail0);

	// get space to store output, e.g., for processing reads
	gclh->revb = evbuffer_new();
	if (gclh->revb == NULL)
		goto fail1;

	// success
	*pgclh = gclh;
	return estat;

fail1:
	estat = ep_stat_from_errno(errno);

//fail0:
	*pgclh = NULL;
	return estat;
}
