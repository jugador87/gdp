/* vim: set ai sw=4 sts=4 : */

/*
**  These headers are not intended for external use.
*/

gcl_handle_t	*gdp_get_gcl_handle(
				gcl_name_t gcl_name,
				gdp_iomode_t mode);

void		gdp_cache_gcl_handle(
				gcl_handle_t *gclh,
				gdp_iomode_t mode);

void		gdp_drop_gcl_handle(
				gcl_name_t gcl_name,
				gdp_iomode_t mode);

EP_STAT		gdp_init_gcl_cache(void);

const char	*_gdp_proto_cmd_name(uint8_t cmd);
