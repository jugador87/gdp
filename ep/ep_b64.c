/*
**  EP_B64.C --- Base 64 encoding/decoding
*/

#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>

static const char	EncChars[62] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

#define EP_STAT_B64_OVERFLOW		EP_STAT_ABORT
#define EP_STAT_B64_BAD_ENCODING	EP_STAT_ERROR


static int
encmaxline(const char *encoding)
{
	switch (encoding[2] & EP_B64_WRAPMASK)
	{
	  case EP_B64_WRAP64:
		return 64;
	  case EP_B64_WRAP76:
		return 76;
	}
	return INT_MAX;
}


/*
**  EP_B64_ENC_LEN --- return size of encoded binary string
**
**  	Useful for determining how big a buffer to malloc.
**  	This implements 4 * ceiling(bsize / 3) using integer
**  	arithmetic.
*/

size_t
ep_b64_enc_len(size_t bsize, const char *encoding)
{
	int neededlength = ((bsize + 2) / 3) * 4;

	// strip off bytes allocated for final padding
	if (!EP_UT_BITSET(EP_B64_PAD, encoding[2]))
		neededlength -= 2 - ((bsize + 2) % 3);

	// add bytes needed for line wrapping
	neededlength += ((neededlength - 1) / encmaxline(encoding)) * 2;

	return neededlength;
}


/*
**  EP_B64_ENCODE --- encode binary to base64-encoded text
**
**  	The size of the txt buffer should always be at least
**  	ceiling(4/3) the size of the bin input, e.g., a 1-3 byte
**  	input should have a four character output, 4-6 byte inputs
**  	have an eight character output, and so on.  Slight allowances
**  	can be made if padding is turned off.
**
**  	On good return, the length of the actual output (without the
**  	null terminator) is encoded in the status.
*/

static char
getenc(int b, const char *encoding)
{
	EP_ASSERT_REQUIRE(b >= 0 && b < 64);

	if (b == 62)
		return encoding[0];
	if (b == 63)
		return encoding[1];
	return EncChars[b];
}

EP_STAT
ep_b64_encode(const void *bbin, size_t bsize,
		char *txt, size_t tsize,
		const char *encoding)
{
	const uint8_t *bin = bbin;
	int maxline;
	int bx, tx;			// indexes into binary & text
	int lx;				// index into current output line
	int nextc;
	int neededlength = ep_b64_enc_len(bsize, encoding) + 1;
					// +1 for null terminator on string

	if (tsize < neededlength)
	{
		ep_dbg_printf("B64_OVERFLOW: tsize = %ld, neededlength = %d\n",
				tsize, neededlength);
		return EP_STAT_B64_OVERFLOW;
	}
	maxline = encmaxline(encoding);

	for (bx = tx = lx = 0; bx < bsize;)
	{
		if (lx >= maxline)
		{
			txt[tx++] = '\r';
			txt[tx++] = '\n';
			lx = 0;
		}
		switch (bx % 3)
		{
		  case 0:
			txt[tx++] = getenc(bin[bx] >> 2, encoding);
			lx++;
			nextc = (bin[bx++] & 0x03) << 4;
			break;

		  case 1:
			nextc |= (bin[bx] & 0xf0) >> 4;
			txt[tx++] = getenc(nextc, encoding);
			lx++;
			nextc = (bin[bx++] & 0x0f) << 2;
			break;

		  case 2:
			nextc |= (bin[bx] & 0xc0) >> 6;
			txt[tx++] = getenc(nextc, encoding);
			txt[tx++] = getenc(bin[bx++] & 0x3f, encoding);
			lx += 2;
			break;
		}
	}

	// insert final output character
	switch (bx % 3)
	{
	  case 0:
		// no additional data to push
		break;

	  case 1:
		txt[tx++] = getenc(nextc, encoding);
		if (EP_UT_BITSET(EP_B64_PAD, encoding[2]))
		{
			txt[tx++] = '=';
			txt[tx++] = '=';
		}
		break;

	  case 2:
		txt[tx++] = getenc(nextc, encoding);
		if (EP_UT_BITSET(EP_B64_PAD, encoding[2]))
			txt[tx++] = '=';
		break;
	}

	// success!
	EP_ASSERT_ENSURE(tx < tsize);
	txt[tx] = '\0';
	return EP_STAT_FROM_INT(tx);
}


/*
**  EP_B64_DECODE --- convert text encoding back into binary
**
**  	The "decoding" map converts text characters to six-bit binary
**  	chunks.  The special value -1 means that it is an illegal
**  	character and -2 means it should be ignored.
*/

EP_STAT
ep_b64_decode(const char *txt, size_t tsize,
		void *bbin, size_t bsize,
		const char *encoding)
{
	uint8_t *bin = bbin;
	int8_t decode[256];
	int bx, tx;
	int state;	// logical position in input, ignoring noise chars
	int nextb = 0;

	// initialize the decode map
	if (EP_UT_BITSET(EP_B64_IGNCRUD, encoding[2]))
		tx = -2;
	else
		tx = -1;
	for (bx = 0; bx < sizeof decode; bx++)
		decode[bx] = tx;
	for (bx = 0; bx < sizeof EncChars; bx++)
		decode[(int) EncChars[bx]] = bx;
	decode[(int) encoding[0]] = 62;
	decode[(int) encoding[1]] = 63;
	if ((encoding[2] & EP_B64_WRAPMASK) != 0)
		decode['\r'] = decode['\n'] = -2;
	if (EP_UT_BITSET(EP_B64_PAD, encoding[2]))
		decode['='] = -2;

	// TODO: check to make sure output buffer won't overflow

	// do the actual decoding
	for (state = tx = bx = 0; tx < tsize && txt[tx] != '\0'; )
	{
		int v = decode[txt[tx++] & 0xff];

		if (v == -1)
			return EP_STAT_B64_BAD_ENCODING;
		else if (v == -2)
			continue;

		switch (state % 4)
		{
		case 0:
			nextb = v << 2;
			break;

		case 1:
			bin[bx++] = nextb | ((v & 0x30) >> 4);
			nextb = (v & 0x0f) << 4;
			break;

		case 2:
			bin[bx++] = nextb | ((v & 0x3c) >> 2);
			nextb = (v & 0x03) << 6;
			break;

		case 3:
			bin[bx++] = nextb | (v & 0x3f);
			break;
		}
		state++;
	}

	// success!
	return EP_STAT_FROM_INT(bx);
}
