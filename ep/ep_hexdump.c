/* vim: set ai sw=8 sts=8 ts=8 :*/

#include <ep.h>
#include <stdio.h>

void
ep_hexdump(void *bufp, size_t buflen, FILE *fp, int format)
{
	size_t offset;
	int i = 0;
	uint8_t *b = bufp;

	for (offset = 0; offset < buflen; offset++)
	{
		if (i == 0)
			fprintf(fp, "%08zx", offset);
		fprintf(fp, " %02x", b[offset]);
		i = (i + 1) % 16;
		if (i == 0)
			fprintf(fp, "\n");
	}
	if (i != 0)
		fprintf(fp, "\n");
}
