/* vim: set ai sw=4 sts=4 : */

/*
**  Headers for the physical log implementation.
**	This is how bytes are actually laid out on the disk.
*/

EP_STAT		gcl_offset_cache_init(
			gcl_handle_t *gclh);

EP_STAT		gcl_read(
			gcl_handle_t *gclh,
			long msgno,
			gdp_msg_t *msg,
			struct evbuffer *evb);

EP_STAT		gcl_create(
			gcl_name_t gcl_name,
			gcl_handle_t **pgclh);

EP_STAT		gcl_open(
			gcl_name_t gcl_name,
			gdp_iomode_t mode,
			gcl_handle_t **pgclh);

EP_STAT		gcl_close(
			gcl_handle_t *gclh);

EP_STAT		gcl_append(
			gcl_handle_t *gclh,
			gdp_msg_t *msg);

