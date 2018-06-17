#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "modem.h"

#include "modemconfig.h"
#include "complex.h"
#include "fec.h"
#include "filter.h"
#include "newqpsktx.h"
#include "tbl.h"
#include "misc.h"

/* --------------------------------------------------------------------- */

static void txtune(void *);
static void txsync(void *);
static void txpredata(void *);
static void txdata(void *);
static void txpostdata(void *);
static void txjam(void *);
static void txflush(void *);

/* --------------------------------------------------------------------- */

void init_newqpsktx(void *state)
{
	struct txstate *s = (struct txstate *)state;
	int i;

	/* switch to tune mode */
	s->txroutine = txtune;
	s->statecntr = s->tunelen;
	s->txwindowfunc = ToneWindowOut;

	/* copy initial tune vectors */
	for (i = 0; i < TuneCarriers; i++) {
		s->tunevect[i].re = TuneIniVectI[i];
		s->tunevect[i].im = TuneIniVectQ[i];
	}
}

/* --------------------------------------------------------------------- */

static int getbyte(void *state, unsigned char *buf)
{
	struct txstate *s = (struct txstate *)state;

	if (s->saved != -1) {
		*buf = (unsigned char) s->saved;
		s->saved = -1;
		return 1;
	}
	return pktget(s->chan, buf, 1);
}

static unsigned int getbitbatch(void *state)
{
	struct txstate *s = (struct txstate *)state;
	unsigned int i, bit, data = 0;
	unsigned char buf;

	for (i = 0; i < s->fec.bitbatchlen; i++) {
		if (s->shreg <= 1) {
			if (!getbyte(s, &buf))
				break;
			s->shreg = buf;
			s->shreg |= 0x100;
		}
		bit = s->shreg & 1;
		s->shreg >>= 1;
		data |= bit << i;
	}

	if (i == s->fec.bitbatchlen)
		s->empty = 0;
	else
		s->empty = 1;

	return data;
}

/* --------------------------------------------------------------------- */

static void fft(complex * in, complex * out)
{
	int i, j, k;
	int s, sep, width, top, bot;
	float tr, ti;

	/* order the samples in bit reverse order */
	for (i = 0; i < WindowLen; i++) {
		j = rbits8(i) >> (8 - WindowLenLog);
		out[j] = in[i];
	}

	/* in-place FFT */
	sep = 1;
	for (s = 1; s <= WindowLenLog; s++) {
		width = sep;	/* butterfly width =  2^(s-1) */
		sep <<= 1;	/* butterfly separation = 2^s */

		for (j = 0; j < width; j++) {

			k = WindowLen * j / sep;

			for (top = j; top < WindowLen; top += sep) {
				bot = top + width;

				tr = out[bot].re * CosTable[k] + out[bot].im * SinTable[k];
				ti = out[bot].im * CosTable[k] - out[bot].re * SinTable[k];

				out[bot].re = out[top].re - tr;
				out[bot].im = out[top].im - ti;

				out[top].re = out[top].re + tr;
				out[top].im = out[top].im + ti;
			}
		}
	}
}

/* --------------------------------------------------------------------- */

int newqpsktx(void *state, complex *samples)
{
	struct txstate *s = (struct txstate *)state;
	complex tmp[WindowLen];
	int i;

	/* clear all FFT bins */
	for (i = 0; i < WindowLen; i++) {
		s->fftbuf[i].re = 0.0;
		s->fftbuf[i].im = 0.0;
	}

	/* process the data */
	if (!s->tuneonly)
		s->txroutine(state);
	else
		txtune(state);

	/* fft */
	fft(s->fftbuf, tmp);

	/* overlap and apply the window function */
	i = 0;
	while (i < WindowLen - SymbolLen) {
		s->txwin[i] = s->txwin[i + SymbolLen];
		s->txwin[i].re += tmp[i].re * s->txwindowfunc[i];
		s->txwin[i].im += tmp[i].im * s->txwindowfunc[i];
		i++;
	}
	while (i < WindowLen) {
		s->txwin[i].re = tmp[i].re * s->txwindowfunc[i];
		s->txwin[i].im = tmp[i].im * s->txwindowfunc[i];
		i++;
	}

	/* output filter and interpolation */
	i = filter(&s->filt, s->txwin, samples);

	return i;
}

/* --------------------------------------------------------------------- */

/*
 * Send tune carriers (four continuous carriers spaced at 600Hz).
 */
static void txtune(void *state)
{
	struct txstate *s = (struct txstate *)state;
	int i, j;

	j = FirstTuneCarr;
	for (i = 0; i < TuneCarriers; i++) {
		/* flip odd carriers -> continuous phase */
		if (j % 2) {
			s->fftbuf[j].re = -s->tunevect[i].re;
			s->fftbuf[j].im = -s->tunevect[i].im;
		} else {
			s->fftbuf[j].re = s->tunevect[i].re;
			s->fftbuf[j].im = s->tunevect[i].im;
		}
		s->tunevect[i] = s->fftbuf[j];
		j += TuneCarrSepar;
	}

	if (!s->tuneonly && --s->statecntr <= 0) {
		/* switch to sync mode */
		s->txroutine = txsync;
		s->statecntr = s->synclen;
		s->txwindowfunc = DataWindowOut;
	}
}

/* --------------------------------------------------------------------- */

/*
 * Send sync sequence (tune carriers turned by 180 degrees every symbol).
 */
static void txsync(void *state)
{
	struct txstate *s = (struct txstate *)state;
	int i, j;

	j = FirstTuneCarr;
	for (i = 0; i < TuneCarriers; i++) {
		/* flip even carriers -> inverted phase */
		if (j % 2) {
			s->fftbuf[j].re = s->tunevect[i].re;
			s->fftbuf[j].im = s->tunevect[i].im;
		} else {
			s->fftbuf[j].re = -s->tunevect[i].re;
			s->fftbuf[j].im = -s->tunevect[i].im;
		}
		s->tunevect[i] = s->fftbuf[j];
		j += TuneCarrSepar;
	}

	if (--s->statecntr <= 0) {
		/* switch to pre data mode */
		s->txroutine = txpredata;
		s->statecntr = TxPreData;
		/* copy initial data vectors */
		for (i = 0; i < DataCarriers; i++) {
			s->datavect[i].re = DataIniVectI[i];
			s->datavect[i].im = DataIniVectQ[i];
		}
		/* initialise the interleaver */
		init_inlv(&s->fec);
	}
}

/* --------------------------------------------------------------------- */

static void encodeword(void *state, int jam)
{
	struct txstate *s = (struct txstate *)state;
	unsigned i, j, k, data;
	complex z;
	float phi;

	/* run through interleaver only if not jamming */
	if (!jam) {
		for (i = 0; i < SymbolBits; i++)
			s->txword[i] = inlv(&s->fec, s->txword[i]);
	}

	j = FirstDataCarr;
	for (i = 0; i < DataCarriers; i++) {
		/* collect databits for this symbol */
		data = 0;
		for (k = 0; k < SymbolBits; k++) {
			data <<= 1;
			if (s->txword[k] & (1 << i))
				data |= 1;
		}

		/* gray encode */
		data ^= data >> 1;

		/* flip the top bit */
		data ^= 1 << (SymbolBits - 1);

		/* modulate */
		phi = data * 2 * M_PI / PhaseLevels;

		/* if jamming, turn by maximum phase error */
		if (jam)
			phi += M_PI / PhaseLevels;

		z.re = cos(phi);
		z.im = sin(phi);
		s->fftbuf[j] = cmul(s->datavect[i], z);

		/* turn odd carriers by 180 degrees */
		if (j % 2) {
			s->fftbuf[j].re = -s->fftbuf[j].re;
			s->fftbuf[j].im = -s->fftbuf[j].im;
		}

		s->datavect[i] = s->fftbuf[j];
		j += DataCarrSepar;
	}
}

static void txpredata(void *state)
{
	struct txstate *s = (struct txstate *)state;
	int i;

	for (i = 0; i < SymbolBits; i++)
		s->txword[i] = 0;

	encodeword(state, 0);

	if (--s->statecntr <= 0) {
		/* switch to data mode */
		s->txroutine = txdata;
	}
}

static void txdata(void *state)
{
	struct txstate *s = (struct txstate *)state;
	int i;

	for (i = 0; i < SymbolBits; i++) {
		s->txword[i] = getbitbatch(state);
		s->txword[i] = fecencode(&s->fec, s->txword[i]);
	}

	encodeword(state, 0);

	if (s->empty) {
		/* switch to post data mode */
		s->txroutine = txpostdata;
		s->statecntr = TxPostData + (s->fec.inlv * DataCarriers + 1) / SymbolBits;
	}
}

static void txpostdata(void *state)
{
	struct txstate *s = (struct txstate *)state;
	int i;

	for (i = 0; i < SymbolBits; i++)
		s->txword[i] = 0;	

	encodeword(state, 0);

#if 0
	if (!hdlc_txempty(&s->hdlc)) {
		/* there is new data - switch back to data mode */
		s->txroutine = txdata;
		return;
	}
#endif

	if (--s->statecntr <= 0) {
		/* switch to jamming mode */
		s->txroutine = txjam;
		s->statecntr = TxJamLen;
		srand(time(NULL));
	}
}

static void txjam(void *state)
{
	struct txstate *s = (struct txstate *)state;
	int i;

	for (i = 0; i < SymbolBits; i++)
		s->txword[i] = rand();

	encodeword(state, 1);

	if (--s->statecntr <= 0) {
		/* switch to buffer flush mode */
		s->txroutine = txflush;
		s->statecntr = 3;
	}
}

static void txflush(void *state)
{
	struct txstate *s = (struct txstate *)state;

	if (--s->statecntr <= 0) {
		/* get ready for the next transmission */
		init_newqpsktx(state);
		/* signal the main routine we're done */
		s->txdone = 1;
	}
}

/* --------------------------------------------------------------------- */
