/*****************************************************************************/

/*
 *      modempsp.c  --  Linux Userland Soundmodem FSK PSP VE enhanced demodulator.
 *
 *      Copyright (C) 1999-2000, 2003
 *        Thomas Sailer (t.sailer@alumni.ethz.ch)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 */

/*****************************************************************************/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "modem.h"

#include <stdio.h>

#include "raisedcosine.h"

/* --------------------------------------------------------------------- */

extern double df9ic_rxfilter(double t);
extern double df9ic_txfilter(double t);

/* --------------------------------------------------------------------- */

#define RCOSALPHA   (3.0/8)

#define FILTERRELAX 1.4

#define DESCRAM17_TAPSH1  0
#define DESCRAM17_TAPSH2  5
#define DESCRAM17_TAPSH3  17

/* --------------------------------------------------------------------- */

#include "psp.h"

/* --------------------------------------------------------------------- */

static inline int isqr(int x)
{
	return x * x;
}

static inline u_int8_t rev_nibble(u_int8_t in)
{
	u_int8_t nibbletab[16] = {0x0,0x8,0x4,0xc,0x2,0xa,0x6,0xe,0x1,0x9,0x5,0xd,0x3,0xb,0x7,0xf};
	return nibbletab[in & 0xf];
}

static inline u_int8_t rev_byte(u_int8_t in) {
	return (rev_nibble(in) << 4) | rev_nibble(in >> 4);
}

#ifdef __i386__

static inline int16_t fir(const int32_t *p1, const int16_t *p2, int len)
{
	int32_t sum, temp;

	__asm__("\n0:\n\t"
		"movswl (%4),%1\n\t"
		"imull (%3),%1\n\t"
		"addl $-2,%4\n\t"
		"addl $4,%3\n\t"
		"addl %1,%0\n\t"
		"decl %2\n\t"
		"jnz 0b\n\t"
		: "=r" (sum), "=r" (temp), "=r" (len), "=S" (p1), "=D" (p2) 
		: "0" (0), "2" (len), "3" (p1), "4" (p2));
	return sum >> 16;
}

#else

static inline int16_t fir(const int32_t *p1, const int16_t *p2, int len)
{
	int32_t sum = 0;

	for(; len > 0; len--, p1++, p2--)
		sum += ((int32_t)*p1) * ((int32_t)*p2);
	return sum >> 16;
}

#endif

/* --------------------------------------------------------------------- */

#define max(a, b) (((a) > (b)) ? (a) : (b))

#define MAXFIRLEN      64U
#define FILTEROVER     16U
#define FILTERSPANBITS  8U

#define WHICHFILTER(x) (((x)>>12)&0xFU)   /* must correspond to FILTEROVER */
#define WHICHSAMPLE(x) ((x)>>16)

#define SNRESTSHIFT 7

#define PARAMSMOOTH 0.95

#define PARSMMUL1   ((int)(PARAMSMOOTH*(1<<16)))
#define PARSMMUL2   ((1<<16)-PARSMMUL1)

struct venode {
	unsigned int metric;
	unsigned int data;
};

struct demodstate {
        struct modemchannel *chan;
	unsigned int filtermode;
	unsigned int bps, firlen;
	unsigned int pllinc, pllcorr;
	int pll;
	u_int16_t stime;

	unsigned int descram;
	unsigned int shistcnt;
	unsigned int snracc, snrcnt;
	int16_t shist[16];

	/*
	 * 0: DC level
	 * 1: postcursor of previous bit
	 * 2: bit amplitude
	 * 3: precursor of following bit
	 */
	int16_t params[4];

	struct venode venodes[8];

	int32_t filter[FILTEROVER][MAXFIRLEN];
};

#define DCD_TIME_SHIFT 7
#define DCD_TIME (1<<DCD_TIME_SHIFT)

static int16_t filter(struct demodstate *s, int16_t *samples, int ph)
{
	return fir(s->filter[WHICHFILTER(ph)], samples + WHICHSAMPLE(ph), s->firlen);
}

static const struct modemparams demodparams[] = {
        { "bps", "Bits/s", "Bits per second", "9600", MODEMPAR_NUMERIC, { n: { 4800, 38400, 100, 1200 } } },
	{ "filter", "Filter Curve", "Filter Curve", "df9ic/g3ruh", MODEMPAR_COMBO, 
	  { c: { { "df9ic/g3ruh", "rootraisedcosine", "raisedcosine", "hamming" } } } },
        { NULL }
        
};

static void *demodconfig(struct modemchannel *chan, unsigned int *samplerate, const char *params[])
{
        struct demodstate *s;
	unsigned int i;

        if (!(s = calloc(1, sizeof(struct demodstate))))
                logprintf(MLOG_FATAL, "out of memory\n");
        s->chan = chan;
        if (params[0]) {
                s->bps = strtoul(params[0], NULL, 0);
                if (s->bps < 4800)
                        s->bps = 4800;
                if (s->bps > 38400)
                        s->bps= 38400;
        } else
                s->bps = 9600;
	s->filtermode = 0;
	if (params[1]) {
		for (i = 1; i < 4; i++)
			if (!strcmp(params[1], demodparams[1].u.c.combostr[i])) {
				s->filtermode = i;
				break;
			}
	}
	*samplerate = s->bps + s->bps / 2;
	return s;
}

static inline int estparams(struct demodstate *s, unsigned int deschist)
{
	int16_t params[ESTPARAMS];
	const int16_t *shist, *sptr, *cptr;
	int sum;
	unsigned int i, j;

	deschist &= ESTDATAMASK;
	if (estsingular[deschist >> 5] & (1 << (deschist & 31))) {
		logprintf(250, "singular pattern\n");
		return 0;
	}
	shist = s->shist + s->shistcnt + ESTSYMBOLS - 1;
	cptr = estmat[deschist];
	for (i = 0; i < ESTPARAMS; i++) {
		sptr = shist;
		sum = 0;
		for (j = 0; j < ESTSYMBOLS; j++, sptr--, cptr++)
			sum += (int)(*sptr) * (int)(*cptr);
		params[i] = sum >> ESTSHIFT;
		sum = PARSMMUL1 * s->params[i] + PARSMMUL2 * params[i];
		s->params[i] = sum >> 16;
	}
	/* clamp params to sane values */
	if (s->params[2] < 250)
		s->params[2] = 250;
	sum = s->params[2] >> 1;
	if (abs(s->params[1]) > sum)
		s->params[1] = (s->params[1] > 0) ? sum : -sum;
	if (abs(s->params[3]) > sum)
		s->params[3] = (s->params[3] > 0) ? sum : -sum;
	logprintf(250, "Par: est: %5d %5d %5d %5d  sm: %5d %5d %5d %5d\n",
		  params[0], params[1], params[2], params[3],
		  s->params[0], s->params[1], s->params[2], s->params[3]);
	return 1;
}

static inline void viterbieq(struct demodstate *s, int16_t curs)
{
	int blevel = s->params[0] - s->params[1] - s->params[2] - s->params[3] - curs;
	int ampl0 = 2 * s->params[1];
	int ampl1 = 2 * s->params[2];
	int ampl2 = 2 * s->params[3];
	unsigned int metric0, metric1;
	struct venode *node1, *node2;
	unsigned char ch;
	unsigned int i;
	float snr;

	if (s->shistcnt & 1) {
		node1 = s->venodes+4;
		node2 = s->venodes;
	} else {
		node1 = s->venodes;
		node2 = s->venodes+4;
	}
	s->shist[s->shistcnt++] = curs;
	s->shistcnt &= 15;
	/* unrolled viterbi equalizer loop */
	metric0 = node1[0].metric + isqr(blevel);
	metric1 = node1[2].metric + isqr(blevel + ampl2);
	if (metric0 < metric1) {
		node2[0].metric = metric0;
		node2[0].data = node1[0].data << 1;
	} else {
		node2[0].metric = metric1;
		node2[0].data = node1[2].data << 1;
	} 
	metric0 = node1[0].metric + isqr(blevel + ampl0);
	metric1 = node1[2].metric + isqr(blevel + ampl2 + ampl0);
	if (metric0 < metric1) {
		node2[1].metric = metric0;
		node2[1].data = (node1[0].data << 1) | 1;
	} else {
		node2[1].metric = metric1;
		node2[1].data = (node1[2].data << 1) | 1;
	} 
	metric0 = node1[1].metric + isqr(blevel + ampl1);
	metric1 = node1[3].metric + isqr(blevel + ampl2 + ampl1);
	if (metric0 < metric1) {
		node2[2].metric = metric0;
		node2[2].data = node1[1].data << 1;
	} else {
		node2[2].metric = metric1;
		node2[2].data = node1[3].data << 1;
	}
	metric0 = node1[1].metric + isqr(blevel + ampl1 + ampl0);
	metric1 = node1[3].metric + isqr(blevel + ampl2 + ampl1 + ampl0);
	if (metric0 < metric1) {
		node2[3].metric = metric0;
		node2[3].data = (node1[1].data << 1) | 1;
	} else {
		node2[3].metric = metric1;
		node2[3].data = (node1[3].data << 1) | 1;
	} 
	/* end of viterbi "loop"; special actions periodically */
	if (s->shistcnt & 7)
		return;
	/* find node with best metric */
	metric0 = ~0;
	metric1 = 0;
	for (i = 0; i < 4; i++)
		if (node2[i].metric < metric0) {
			metric0 = node2[i].metric;
			metric1 = node2[i].data;
		}
	/* decide data, send to decoder */
	s->descram <<= 8;
	s->descram |= (metric1 >> 16) & 0xff; /* 16 is about 5*constraint length */
	i = ~(s->descram ^ (s->descram >> 1));
	i ^= (i >> DESCRAM17_TAPSH3) ^ (i >> (DESCRAM17_TAPSH3-DESCRAM17_TAPSH2));
	ch = rev_byte(i);
	pktput(s->chan, &ch, 1);
	if (logcheck(257)) {
		char buf2[9];
		for (i = 0; i < 8; i++)
			buf2[i] = '0' + ((ch >> i) & 1);
		buf2[8] = 0;
		logprintf(257, "fskrx: %s\n", buf2);
	}
	/* subtract metric */
	s->snracc += metric0 >> SNRESTSHIFT;
	for (i = 0; i < 4; i++) {
		node2[i].metric -= metric0;
		if (node2[i].metric > 0x7fffffff)
			node2[i].metric = 0x7fffffff;   /* prevent overflow */
	}
	/* new parameter estimate */
	estparams(s, metric1 >> (16-ESTSYMBOLS));
	/* SNR estimate */
	s->snrcnt++;
	if (s->snrcnt < (1<<(SNRESTSHIFT-3)))
		return;
	s->snrcnt = 0;
	metric0 = isqr(s->params[1]) + isqr(s->params[2]) + isqr(s->params[3]);
	metric1 = metric0;
	if (!metric1)
		metric1 = 1;
	snr = -10*M_LOG10E*log(s->snracc / (float)metric1);
	logprintf(128, "SNR: signal power %u noise+interference pwr %u  S/(N+I) %6.2fdB\n", metric0, s->snracc, snr);
	pktsetdcd(s->chan, s->snracc < (metric0 >> 2));
	s->snracc = 0;
}

static void demodrx(struct demodstate *s, unsigned nsamples)
{
	int16_t *samples;
 	int16_t curs, nexts, mids;
	int32_t gardner;
	int corr;

	samples = alloca((nsamples + s->firlen) * sizeof(samples[0]));
	audioread(s->chan, samples, nsamples + s->firlen, s->stime);
	s->stime += nsamples;
	samples += s->firlen;
	while (WHICHSAMPLE(s->pll + s->pllinc) < nsamples) {
#if 0
		for (corr = 0; corr < 16; corr++)
			printf("%d\n", filter(s, samples, s->pll+corr*s->pllinc/16));
#endif
		curs = filter(s, samples, s->pll);
		mids = filter(s, samples, s->pll+s->pllinc/2);
		nexts = filter(s, samples, s->pll+s->pllinc);
		gardner = ((nexts > 0 ? 1 : -1) - (curs > 0 ? 1 : -1)) * mids;
		s->pll += s->pllinc;
#if 0
		corr = (gardner * s->pllinc) >> 20;
		s->pll -= corr;
#elif 0
		if (gardner < 0)
			s->pll += s->pllinc >> 4;
		else
			s->pll -= s->pllinc >> 4;
#else
		if ((curs > 0) ^ (nexts > 0)) {
			if ((curs > 0) ^ (mids > 0))
				s->pll -= s->pllinc >> 5;
			if ((nexts > 0) ^ (mids > 0))
				s->pll += s->pllinc >> 5;
		}
#endif
		viterbieq(s, curs);
	}
	s->pll -= (nsamples << 16);
}

static void demoddemodulate(void *state)
{
	struct demodstate *s = (struct demodstate *)state;

	s->stime = audiocurtime(s->chan);
	for (;;)
		demodrx(s, 256);
}

static void demodinit(void *state, unsigned int samplerate, unsigned int *bitrate)
{
        struct demodstate *s = (struct demodstate *)state;
	float coeff[FILTEROVER][MAXFIRLEN];
	double tmul;
	float max1, max2, t, at, f1, f2, f3;
	int i, j;

	s->firlen = (samplerate * FILTERSPANBITS + s->bps - 1) / s->bps;
	if (s->firlen > MAXFIRLEN) {
		logprintf(MLOG_WARNING, "demodfsk: input filter length too long\n");
		s->firlen = MAXFIRLEN;
	}
	tmul = ((double)s->bps) / FILTEROVER / ((double)samplerate);
	switch (s->filtermode) {
	case 1:  /* root raised cosine */
		for (i = 0; i < FILTEROVER*s->firlen; i++) {
			t = (signed)(i - s->firlen*FILTEROVER/2) * tmul;
			coeff[((unsigned)i) % FILTEROVER][((unsigned)i) / FILTEROVER] = root_raised_cosine_time(t, RCOSALPHA);
		}
		break;

	case 2:  /* raised cosine */
		for (i = 0; i < FILTEROVER*s->firlen; i++) {
			t = (signed)(i - s->firlen*FILTEROVER/2) * tmul;
			coeff[((unsigned)i) % FILTEROVER][((unsigned)i) / FILTEROVER] = raised_cosine_time(t, RCOSALPHA);
		}
		break;

	case 3:  /* hamming */
		tmul *= FILTERRELAX;
		for (i = 0; i < FILTEROVER*s->firlen; i++)
			coeff[((unsigned)i) % FILTEROVER][((unsigned)i) / FILTEROVER] = 
				sinc((i - (signed)s->firlen*FILTEROVER/2)*tmul)
				* hamming((double)i / (double)(FILTEROVER*s->firlen-1));
		break;

	default:  /* DF9IC */
		for (i = 0; i < FILTEROVER*s->firlen; i++) {
			t = (signed)(i - s->firlen*FILTEROVER/2) * tmul;
			coeff[((unsigned)i) % FILTEROVER][((unsigned)i) / FILTEROVER] = df9ic_rxfilter(t);
		}
		break;
	}
	max1 = 0;
	for (i = 0; i < FILTEROVER; i++) {
		max2 = 0;
		for (j = 0; j < s->firlen; j++)
			max2 += fabs(coeff[i][j]);
		if (max2 > max1)
			max1 = max2;
	}
	max2 = ((float)0x7fffffff / (float)0x7fff) / max1;
	for (i = 0; i < FILTEROVER; i++)
		for (j = 0; j < s->firlen; j++)
			s->filter[i][j] = max2 * coeff[i][j];
	s->pllinc = (0x10000 * samplerate + s->bps/2) / s->bps;
	s->pll = 0;
	s->pllcorr = s->pllinc / 8;

	s->shistcnt = s->snracc = s->snrcnt = 0;
	s->params[0] = 0;
	s->params[1] = 300;
	s->params[2] = 11000;
	s->params[3] = 300;
	for (i = 0; i < 8; i++)
		s->venodes[i].metric = 0x7fffffff;
	*bitrate = s->bps;
}

/* --------------------------------------------------------------------- */

struct demodulator fskpspdemodulator = {
	NULL,
	"fskpsp",
	demodparams,
	demodconfig,
	demodinit,
	demoddemodulate,
	free
};

/* --------------------------------------------------------------------- */
