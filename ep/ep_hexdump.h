/* vim: set ai sw=8 sts=8 ts=8 :*/

/*
**  Print binary area to file for human consumption.
**
**	The offset is just added to the printed offset.  For example,
**	if you are printing starting at offset 16 in a larger buffer,
**	this will cause the labels to start at 16 rather than zero.
*/

void	ep_hexdump(
		const void *bufp,	// buffer to print
		size_t buflen,		// length of buffer
		FILE *fp,		// file to print to
		int format,		// format (see below)
		size_t offset);		// starting offset of bufp

// format flags
#define EP_HEXDUMP_HEX		0	// no special formatting
#define EP_HEXDUMP_ASCII	0x0001	// show ASCII equivalent
