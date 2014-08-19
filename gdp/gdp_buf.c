/* vim: set ai sw=4 sts=4 ts=4 :*/

#include "gdp.h"
#include "gdp_buf.h"

void
gdp_buf_dump(gdp_buf_t *buf, FILE *fp)
{
	fprintf(fp, "gdp_buf @ %p: len=%zu\n",
			buf, gdp_buf_getlength(buf));
}
