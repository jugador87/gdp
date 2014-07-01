/* vim: set ai sw=8 sts=8 :*/

/*
**  EP_B64.H --- Base 64 encoding/decoding
**
**	There are multiple ways of encoding Base64 depending on use.
**	Pretty much everyone agrees that values 0-61 are from the
**	set [A-Za-z0-9], but 62 and 63 are special characters which
**	vary based on context.  The "encoding" parameter is two or three
**	characters.  The first two are the encoding for those last two
**	values, and the last optional character represents flags as
**	a binary value added to the '@' character (so @ = 0, A = 1,
**	etc.)
**
**	XXX need to tell line length for encoding and whether other
**	characters are ignored for decoding.
*/

#ifndef _EP_B64_H_
#define _EP_B64_H_

#include <ep/ep.h>

extern EP_STAT	ep_b64_encode(const void *bin,		// raw binary input
				size_t bsize,		// sizeof bin
				char *txt,		// text output buffer
				size_t tsize,		// size of txt
				const char *encoding);	// encoding
extern EP_STAT	ep_b64_decode(const char *txt,		// text input
				size_t tsize,		// sizeof txt
				void *bin,		// output binary buffer
				size_t bsize,		// sizeof bin
				const char *encoding);	// encoding

// flag bits; only the bottom six bits are available; bottom two bits
// encode wrap length
#define EP_B64_NOWRAP		0x00	// never wrap lines
#define EP_B64_WRAP64		0x01	// wrap at 64 characters
#define EP_B64_WRAP76		0x02	// wrap at 76 characters
#define EP_B64_WRAPMASK		0x03	// bit mask for wrapping
#define EP_B64_PAD		0x04	// pad with '='
#define EP_B64_IGNCRUD		0x08	// ignore unrecognized chars

// encodings for common standards
#define EP_B64_ENC_MIME		"+/N"	// WRAP76  PAD  IGNCRUD
#define EP_B64_ENC_PEM		"+/E"	// WRAP64  PAD -IGNCRUD
#define EP_B64_ENC_URL		"-_@"	// NOWRAP -PAD -IGNCRUD

#endif // _EP_B64_H_
