/* vim: set ai sw=8 sts=8 ts=8 :*/

/*
**  The format parameter tweaks the output.
*/

#define EP_HEXDUMP_ASCII	0x0001	// show ASCII equivalent

void	ep_hexdump(const void *bufp, size_t buflen, FILE *fp, int format);
