#include "modemconfig.h"
#include "fec.h"
#include "fectable.h"
#include "bch.h"
#include "misc.h"

/* --------------------------------------------------------------------- */

unsigned deinlv(struct fecstate *f, unsigned in)
{
	unsigned i, ptr, out;

	if (!f->inlv)
		return in;

	out = 0;
	ptr = f->inlvptr;

	for (i = 0; i < DataCarriers; i++) {
		out |= f->inlvpipe[ptr] & InterleavePattern[i];
		ptr = (ptr + f->inlv) % (f->inlv * DataCarriers);
	}

	f->inlvpipe[f->inlvptr] = in;
	f->inlvptr = (f->inlvptr + 1) % (f->inlv * DataCarriers);

	return out;
}

unsigned inlv(struct fecstate *f, unsigned in)
{
	unsigned i, ptr, out;

	if (!f->inlv)
		return in;

	ptr = f->inlvptr;

	for (i = 0; i < DataCarriers; i++) {
		f->inlvpipe[ptr] |= in & InterleavePattern[i];
		ptr = (ptr + f->inlv) % (f->inlv * DataCarriers);
	}

	out = f->inlvpipe[f->inlvptr];
	f->inlvpipe[f->inlvptr] = 0;
	f->inlvptr = (f->inlvptr + 1) % (f->inlv * DataCarriers);

	return out;
}

/* --------------------------------------------------------------------- */

static unsigned fec1511encode(unsigned in)
{
	unsigned i, out;

	in &= 0x7ff;
	out = 0;

	for (i = 0; i < 11; i++)
		if (in & (1 << i))
			out ^= FEC1511EncodeTable[i];

	return in | out;
}

static unsigned fec1511decode(unsigned in, unsigned *err)
{
	unsigned crc;

	crc = fec1511encode(in);
	crc ^= in;
	crc >>= 11;
	*err = FEC1511DecodeTable[crc];

	return *err ^ in;
}

/* --------------------------------------------------------------------- */

static unsigned walshencode(unsigned in)
{
	return WalshTable[in & 0x1f];
}

static unsigned walshdecode(unsigned in, unsigned *err)
{
	unsigned i, out, diff, dist, best;

	out = 0;
	best = 16;

	for (i = 0; i < 32; i++) {
		diff = in ^ WalshTable[i];
		dist = hweight16(diff);
		if (dist < best) {
			out = i;
			best = dist;
			*err = diff;
		}
	}

	return out;
}

/* --------------------------------------------------------------------- */

unsigned fecdecode(struct fecstate *f, unsigned data, unsigned *errors)
{
	switch (f->feclevel) {
	case 0:
		errors = 0;
		break;
	case 1:
		data = fec1511decode(data, errors);
		break;
	case 2:
		data = decode_bch_codeword(data, errors);
		break;
	case 3:
		data = walshdecode(data, errors);
		break;
	}
	return data;
}

unsigned fecencode(struct fecstate *f, unsigned data)
{
	switch (f->feclevel) {
	case 0:
		break;
	case 1:
		data = fec1511encode(data);
		break;
	case 2:
		data = encode_bch_codeword(data);
		break;
	case 3:
		data = walshencode(data);
		break;
	}
	return data;
}

/* --------------------------------------------------------------------- */

