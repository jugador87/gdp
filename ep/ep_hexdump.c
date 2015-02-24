/* vim: set ai sw=8 sts=8 ts=8 :*/

#include <ep.h>
#include <ep_hexdump.h>
#include <ep_string.h>

#include <ctype.h>
#include <stdio.h>

void
ep_hexdump(const void *bufp, size_t buflen, FILE *fp,
		int format, size_t offset)
{
	size_t bufleft = buflen;
	const uint8_t *b = bufp;
	const size_t width = 16;

	flockfile(fp);			// to make threads happy
	while (bufleft > 0)
	{
		int lim = bufleft;
		int i;
		int shift;

		shift = offset % width;
		if (lim > width)
			lim = width;
		if (lim > (width - shift))
			lim = width - shift;
		fprintf(fp, "%08zx", offset);
		if (shift != 0)
			fprintf(fp, "%*s", shift * 3, "");
		for (i = 0; i < lim; i++)
			fprintf(fp, " %02x", b[i]);
		if (EP_UT_BITSET(EP_HEXDUMP_ASCII, format))
		{
			fprintf(fp, "\n        ");
			if (shift != 0)
				fprintf(fp, "%*s", shift * 3, "");
			for (i = 0; i < lim; i++)
			{
				if (isprint(b[i]))
					fprintf(fp, " %c ", b[i]);
				else
					fprintf(fp, " %s ", EpChar->unprintable);
			}
		}
		fprintf(fp, "\n");
		b += lim;
		bufleft -= lim;
		offset += lim;
	}
	funlockfile(fp);
}
