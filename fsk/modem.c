/*****************************************************************************/

/*
 *      modem.c  --  Linux Userland Soundmodem FSK modulator.
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
#include <pthread.h>
#include "modem.h"

#include <stdio.h>

#include "raisedcosine.h"

/* --------------------------------------------------------------------- */

extern double df9ic_rxfilter(double t);
extern double df9ic_txfilter(double t);

/* --------------------------------------------------------------------- */

#define DESCRAM17_TAPSH1  0
#define DESCRAM17_TAPSH2  5
#define DESCRAM17_TAPSH3  17

#define SCRAM17_TAP1  (1<<DESCRAM17_TAPSH3)
#define SCRAM17_TAPN  ((1<<DESCRAM17_TAPSH1)|(1<<DESCRAM17_TAPSH2))

/* --------------------------------------------------------------------- */

#define FILTERLEN     8U
#define NUMFILTER    32U
#define FILTERIDX(x) (((x) >> 11U) & 31U)

#define RCOSALPHA   (3.0/8)

#define FILTERRELAX 1.4

struct modstate {
        struct modemchannel *chan;
	unsigned int filtermode;
	unsigned int phase, phaseinc, bps;
	unsigned int shreg, txbits, scram;
	int16_t filter[NUMFILTER][FILTERLEN];
};

/* --------------------------------------------------------------------- */

#if defined(__i386__) && 0

static inline int mfilter(unsigned txbits, const int16_t *coeff, int len)
{
	int sum, temp1;

	__asm__("\n1:\n\t"
		"shrl $1,%1\n\t"
		"sbbl %4,%4\n\t"
		"subl %4,%0\n\t"
		"xorl (%2),%4\n\t"
		"addl $2,%2\n\t"
		"addl %4,%0\n\t"
		"decl %3\n\t"
		"jnz 1b\n\t"
		: "=r" (sum), "=r" (txbits), "=r" (coeff), "=r" (len), "=r" (temp1)
		: "0" (0), "1" (txbits), "2" (coeff), "3" (len));
	return sum;
}

#else /* __i386__ */

static inline int32_t mfilter(unsigned txbits, const int16_t *coeff, int len)
{
	int32_t sum = 0;

	for (; len > 0; len--, coeff++, txbits >>= 1)
		if (txbits & 1)
			sum += *coeff;
		else
			sum -= *coeff;
	return sum;
}

#endif /* __i386__ */

/* --------------------------------------------------------------------- */

static const struct modemparams modparams[] = {
        { "bps", "Bits/s", "Bits per second", "9600", MODEMPAR_NUMERIC, { n: { 4800, 38400, 100, 1200 } } },
	{ "filter", "Filter Curve", "Filter Curve", "df9ic/g3ruh", MODEMPAR_COMBO, 
	  { c: { { "df9ic/g3ruh", "rootraisedcosine", "raisedcosine", "hamming" } } } },
        { NULL }
        
};

static void *modconfig(struct modemchannel *chan, unsigned int *samplerate, const char *params[])
{
        struct modstate *s;
	unsigned int i;
        
        if (!(s = calloc(1, sizeof(struct modstate))))
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
			if (!strcmp(params[1], modparams[1].u.c.combostr[i])) {
				s->filtermode = i;
				break;
			}
	}
	*samplerate = s->bps + s->bps / 2;
	return s;
}

static void modinit(void *state, unsigned int samplerate)
{
        struct modstate *s = (struct modstate *)state;
	int i, j;
	float f1, f2, time, alphatime;
	float c[NUMFILTER * FILTERLEN];

	s->phaseinc = (s->bps << 16) / samplerate;
	switch (s->filtermode) {
	case 1:  /* root raised cosine */
		for (i = 0; i < NUMFILTER * FILTERLEN; i++) {
			time = i - (NUMFILTER * FILTERLEN - 1.0) / 2.0;
			time *= (1.0 / NUMFILTER);
			c[i] = root_raised_cosine_time(time, RCOSALPHA);
		}
		break;

	case 2:  /* raised cosine */
		for (i = 0; i < NUMFILTER * FILTERLEN; i++) {
			time = i - (NUMFILTER * FILTERLEN - 1.0) / 2.0;
			time *= (1.0 / NUMFILTER);
			c[i] = raised_cosine_time(time, RCOSALPHA);
		}
		break;

	case 3:  /* hamming */
		for (i = 0; i < NUMFILTER * FILTERLEN; i++) {
			f1 = i - (NUMFILTER * FILTERLEN - 1.0) / 2.0;
			f1 *= (FILTERRELAX / NUMFILTER);
			f2 = i * (1.0 / (NUMFILTER * FILTERLEN - 1.0));
			c[i] = sinc(f1) * hamming(f2);
		}
		break;

	default:  /* DF9IC */
		for (i = 0; i < NUMFILTER * FILTERLEN; i++) {
			time = i - (NUMFILTER * FILTERLEN - 1.0) / 2.0;
			time *= (1.0 / NUMFILTER);
			c[i] = df9ic_txfilter(time);
		}
		break;
	}
	f1 = 0;
	for (i = 0; i < NUMFILTER; i++) {
		for (f2 = 0, j = i; j < NUMFILTER * FILTERLEN; j += NUMFILTER)
			f2 += fabs(c[j]);
		if (f2 > f1)
			f1 = f2;
	}
	f1 = 32767.0 / f1;
	for (i = 0; i < NUMFILTER; i++)
		for (j = 0; j < FILTERLEN; j++)
			s->filter[i][j] = f1 * c[j * NUMFILTER + i];
#if 1
	if (logcheck(258)) {
		char buf[4096];
		char *cp = buf;
		for (i = 0; i < NUMFILTER * FILTERLEN; i++)
			cp += snprintf(cp, buf + sizeof(buf) - cp, " %f", f1 * c[i]);
		logprintf(258, "fsk: txp  = [%s];\n", buf+1);
		for (i = 0; i < NUMFILTER; i++) {
			cp = buf;
			for (j = 0; j < FILTERLEN; j++)
				cp += snprintf(cp, buf + sizeof(buf) - cp, " %d", s->filter[i][j]);
			logprintf(258, "fsk: txp%u = [%s];\n", i, buf+1);
		}
	}
#endif
}

static void modsendbits(struct modstate *s, unsigned char *data, unsigned int nrbits)
{
        int16_t sbuf[512];
        int16_t *sptr = sbuf, *eptr = sbuf + sizeof(sbuf)/sizeof(sbuf[0]);
	unsigned int bitcnt, bits;

	bits = *data++;
	bitcnt = 0;
	while (bitcnt < nrbits) {
		s->phase += s->phaseinc;
		if (s->phase >= 0x10000) {
			s->phase &= 0xffff;
			bitcnt++;
			s->scram = (s->scram << 1) | ((s->scram ^ bits ^ 1) & 1);
			bits >>= 1;
			if (s->scram & (SCRAM17_TAP1 << 1))
				s->scram ^= SCRAM17_TAPN << 1;
			s->txbits = (s->txbits << 1) | ((s->scram >> DESCRAM17_TAPSH3) & 1);
			if (!(bitcnt & 7))
				bits = *data++;
		}
		*sptr++ = mfilter(s->txbits, s->filter[FILTERIDX(s->phase)], FILTERLEN);
		if (sptr >= eptr) {
			audiowrite(s->chan, sbuf, sptr - sbuf);
			sptr = sbuf;
		}
	}
        audiowrite(s->chan, sbuf, sptr - sbuf);
}

static void modmodulate(void *state, unsigned int txdelay)
{
        struct modstate *s = (struct modstate *)state;
        unsigned char ch[8];
        unsigned int i, j;

        i = txdelay * s->bps / 1000;
	if (i < 24)
		i = 24;
	memset(ch, 0x7e, sizeof(ch));
	while (i > 0) {
		j = i;
		if (j > 8*sizeof(ch))
			j = 8*sizeof(ch);
		modsendbits(s, ch, j);
		i -= j;
	}
        while (pktget(s->chan, ch, sizeof(ch)))
                modsendbits(s, ch, 8*sizeof(ch));
	ch[0] = ch[1] = 0x7e;
	modsendbits(s, ch, 16);
}

struct modulator fskmodulator = {
        NULL,
        "fsk",
        modparams,
        modconfig,
        modinit,
        modmodulate,
	free
};

/* --------------------------------------------------------------------- */

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

#define EQLENGTH       5
#define EQGAIN       100

struct demodstate {
        struct modemchannel *chan;
	unsigned int filtermode;
	unsigned int bps, firlen;
	unsigned int pllinc, pllcorr;
	int pll;
	u_int16_t stime;
	
	unsigned int div, divcnt;
	unsigned int shreg, descram;
        int dcd_sum0, dcd_sum1, dcd_sum2;
        unsigned int dcd_time, dcd;
	u_int32_t mean, meansq;
	int32_t dcoffsp;
	unsigned int dcoffscnt;
	int16_t dcoffs;

	unsigned int eqbits;
	int16_t eqs[EQLENGTH], eqf[EQLENGTH];

	int32_t filter[FILTEROVER][MAXFIRLEN];
};

#define DCD_TIME_SHIFT 7
#define DCD_TIME (1<<DCD_TIME_SHIFT)

static int16_t filter(struct demodstate *s, int16_t *samples, int ph)
{
	return fir(s->filter[WHICHFILTER(ph)], samples + WHICHSAMPLE(ph), s->firlen);
}

static int16_t equalizer(struct demodstate *s, int16_t s1, int16_t s2)
{
	int16_t target;
	int32_t sum, corr;
	int i;

	memmove(s->eqs + 2, s->eqs, sizeof(s->eqs) - 2 * sizeof(s->eqs[0]));
	s->eqs[1] = s1;
	s->eqs[0] = s2;
	s->eqbits = (s->eqbits << 1) | (s2 > 0);
	target = (s->eqbits & (1 << ((EQLENGTH-1)/4))) ? 0x4000 : -0x4000;
	for (sum = i = 0; i < EQLENGTH; i++)
		sum += ((int32_t)s->eqs[i]) * ((int32_t)s->eqf[i]);
	sum >>= 14;
	corr = ((target - sum) * EQGAIN) >> 15;
	for (i = 0; i < EQLENGTH; i++)
		s->eqf[i] += (s->eqs[i] * corr) >> 15;
 printf("%5d %5d %5d %5d %5d\n", s->eqf[0], s->eqf[1], s->eqf[2], s->eqf[3], s->eqf[4]);
	return sum;
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

static void demodrx(struct demodstate *s, unsigned nsamples)
{
	int16_t *samples;
 	int16_t curs, nexts, mids, xs, eq;
	int32_t gardner;
	unsigned int d, descx;
	unsigned char ch[3];
	int corr, dcd;

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
		/* dc offset prediction */
		s->dcoffsp += curs;
		s->dcoffscnt++;
		if (s->dcoffscnt >= 4096) {
			s->dcoffs = -(s->dcoffsp >> 12);
			logprintf(257, "fskrx: DC offset %d\n", -s->dcoffs);
			s->dcoffscnt = 0;
			s->dcoffsp = 0;
		}
		/* dc offset compensation */
		curs += s->dcoffs;
		mids += s->dcoffs;
		nexts += s->dcoffs;
		/* sample clock recovery */
		gardner = ((nexts > 0 ? 1 : -1) - (curs > 0 ? 1 : -1)) * mids;
#if 0
		eq = equalizer(s, mids, nexts);
#endif
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

		/* accumulate values for DCD */
		s->mean += abs(curs);
		s->meansq += (((int32_t)curs) * ((int32_t)curs)) >> DCD_TIME_SHIFT;
		/* process sample */
		s->descram <<= 1;
		s->descram |= (curs >> 15) & 1;
		descx = ~(s->descram ^ (s->descram >> 1));
		descx ^= (descx >> DESCRAM17_TAPSH3) ^ (descx >> (DESCRAM17_TAPSH3-DESCRAM17_TAPSH2));
		s->shreg >>= 1;
		s->shreg |= (descx & 1) << 24;
		if (s->shreg & 1) {
			ch[0] = s->shreg >> 1;
			ch[1] = s->shreg >> 9;
			ch[2] = s->shreg >> 17;
			pktput(s->chan, ch, 3);
			if (logcheck(257)) {
				char buf2[25];
				unsigned int i;
				for (i = 0; i < 24; i++)
					buf2[i] = '0' + ((s->shreg >> (i+1)) & 1);
				buf2[24] = 0;
				logprintf(257, "fskrx: %s\n", buf2);
			}
			s->shreg = 0x1000000;
		}
		/* DCD */
		s->dcd_time++;
		if (s->dcd_time < DCD_TIME)
			continue;
		s->mean >>= DCD_TIME_SHIFT;
		s->mean *= s->mean;
                if (s->meansq < 512)
                        dcd = 0;
                else
                        dcd = (s->mean + (s->mean >> 2)) > s->meansq;
		logprintf(256, "DCD: mean: %8u meansq: %8u  diff: %8d  DCD: %u\n", s->mean, s->meansq, s->meansq-s->mean, dcd);
		s->dcd_time = 0;
		pktsetdcd(s->chan, /*(s->dcd_sum0 + s->dcd_sum1 + s->dcd_sum2) < 0*/ dcd);
		s->dcd_sum2 = s->dcd_sum1;
		s->dcd_sum1 = s->dcd_sum0;
		s->dcd_sum0 = 2; /* slight bias */
		s->meansq = s->mean = 0;
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
	float pulseen[FILTEROVER];
	double tmul;
	float max1, max2, t, at, f1, f2;
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
	max2 = ((float)0x3fffffff / (float)0x7fff) / max1;
	for (i = 0; i < FILTEROVER; i++) {
		f1 = 0;
		for (j = 0; j < s->firlen; j++) {
			s->filter[i][j] = max2 * coeff[i][j];
			f1 += s->filter[i][j] * s->filter[i][j];
		}
		pulseen[i] = f1;
	}
#if 1
	if (logcheck(258)) {
		char buf[4096];
		char *cp = buf;
		for (i = 0; i < FILTEROVER*s->firlen; i++)
			cp += snprintf(cp, buf + sizeof(buf) - cp, " %f", coeff[((unsigned)i) % FILTEROVER][((unsigned)i) / FILTEROVER]);
		logprintf(258, "fsk: rxp  = [%s];\n", buf+1);
		for (i = 0; i < FILTEROVER; i++) {
			cp = buf;
			for (j = 0; j < s->firlen; j++)
				cp += snprintf(cp, buf + sizeof(buf) - cp, " %d", s->filter[i][j]);
			logprintf(258, "fsk: rxp%u = [%s];\n", i, buf+1);
		}
	}
#endif
	if (logcheck(257)) {
		char buf[512];
		char *cp = buf;
		for (i = 0; i < FILTEROVER; i++)
			cp += sprintf(cp, ", %6.2gdB", 10*M_LOG10E*log(pulseen[i]) - 10*M_LOG10E*log(32768.0 * (1<<16)));
		logprintf(257, "fsk: rxpulse energies: %s\n", buf+2);
	}
	s->pllinc = (0x10000 * samplerate + s->bps/2) / s->bps;
	s->pll = 0;
	s->pllcorr = s->pllinc / 8;
	s->eqbits = 0;
	memset(s->eqs, 0, sizeof(s->eqs));
	memset(s->eqf, 0, sizeof(s->eqf));
	s->shreg = 0x1000000;
	*bitrate = s->bps;
}

/* --------------------------------------------------------------------- */

struct demodulator fskdemodulator = {
	NULL,
	"fsk",
	demodparams,
	demodconfig,
	demodinit,
	demoddemodulate,
	free
};

/* --------------------------------------------------------------------- */
