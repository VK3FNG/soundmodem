/*****************************************************************************/

/*
 *      p3dmodemsimple.c  --  AO-40 P3D PSK modem.
 *
 *      Copyright (C) 1999-2000
 *        Thomas Sailer (sailer@ife.ee.ethz.ch)
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
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
#pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif

#include "modem.h"
#include "p3d.h"
#include "p3dtbl.h"
#include "simd.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

/* --------------------------------------------------------------------- */

static inline double sinc(double x)
{
        double arg = x * M_PI;

        if (fabs(arg) < 1e-10)
                return 1;
        return sin(arg) / arg;
}

static inline double hamming(double x)
{
        return 0.54-0.46*cos(2*M_PI*x);
}

/* ---------------------------------------------------------------------- */

struct txstate {
        struct modemchannel *chan;
};

static const struct modemparams modparams[] = {
        { NULL }
        
};

static void *modconfig(struct modemchannel *chan, unsigned int *samplerate, const char *params[])
{
        struct txstate *s;
        
        if (!(s = calloc(1, sizeof(struct txstate))))
                logprintf(MLOG_FATAL, "out of memory\n");
        s->chan = chan;
	*samplerate = 8000;
	return s;
}

static void modinit(void *state, unsigned int samplerate)
{
        struct txstate *s = (struct txstate *)state;
}

static void modmodulate(void *state, unsigned int txdelay)
{
	struct txstate *tx = (struct txstate *)state;
}

struct modulator p3dmodulator = {
        NULL,
        "p3d",
        modparams,
        /*modconfig*/NULL,
        /*modinit*/NULL,
        /*modmodulate*/NULL,
	free
};

/* ---------------------------------------------------------------------- */

struct rxfilter {
	int16_t re[RXFILTOVER][RXFILTLEN];
	int16_t im[RXFILTOVER][RXFILTLEN];
};

struct rxstate {
        struct modemchannel *chan;
	unsigned int srate;
	unsigned int rxfiltlen;
	int16_t basebandfilter[RXFILTOVER][RXFILTLEN];
};

/* ---------------------------------------------------------------------- */

/*
 * Compute the filter coefficients
 */
#define RCOSALPHA   0.4
#define FILTERRELAX 1.4

static void compute_rxfilter_from_bb(struct rxstate *rx, struct rxfilter *filter, unsigned int phinc)
{
	unsigned int phase = 0, phase2, phinc2, i, j;

	memset(filter, 0, sizeof(struct rxfilter));
	phinc2 = phinc >> RXFILTOVERBITS;
	//phase += (RXFILTOVER-1) * phinc2;
        for (i = 0; i < rx->rxfiltlen; i++) {
		phase2 = phase;
		for (j = 0; j < RXFILTOVER; j++) {
			filter->re[j][i] = (COS(phase2) * rx->basebandfilter[j][i]) >> 15;
			filter->im[j][i] = -(SIN(phase2) * rx->basebandfilter[j][i]) >> 15;
			phase2 -= phinc2;
		}
		phase += phinc;
	}
}

static inline void compute_rxfilter(struct rxstate *rx)
{
	float coeff[RXFILTOVER][RXFILTLEN] = { { 0, }, };
        float pulseen[RXFILTOVER] = { 0, };
        double tmul;
        float max1, max2, t, tm, at, f1, f2, f3;
        unsigned int i, j;

	memset(coeff, 0, sizeof(coeff));
        rx->rxfiltlen = (rx->srate * RXFILTSPAN + SYMRATE - 1) / SYMRATE;
	/* round to next 4 because SIMD is more efficient with this */
	rx->rxfiltlen = (rx->rxfiltlen + 3) & ~3;
        if (rx->rxfiltlen > RXFILTLEN) {
                logprintf(MLOG_WARNING, "demodp3d: input filter length too long\n");
                rx->rxfiltlen = RXFILTLEN;
        }
	logprintf(257, "p3d: rxfilter length %u, sampling rate %u\n", rx->rxfiltlen, rx->srate);
	tm = rx->rxfiltlen*RXFILTOVER*0.5;
#if 0
        tmul = FILTERRELAX * (2.0 * SYMRATE) / RXFILTOVER / ((double)rx->srate);
        for (i = 0; i < RXFILTOVER*rx->rxfiltlen; i++)
                coeff[i % RXFILTOVER][i / RXFILTOVER] =
			sinc((i - tm) * tmul)
                        * hamming((double)i / (double)(RXFILTOVER*rx->rxfiltlen-1));
#else
        tmul = (2.0*SYMRATE) / RXFILTOVER / ((double)rx->srate);
        for (i = 0; i < RXFILTOVER*rx->rxfiltlen; i++) {
                t = (i - tm) * tmul;
                at = t * RCOSALPHA;
                f1 = 1 - 4 * at * at;
                if (fabs(f1) < 1e-10)
                        f2 = M_PI * (1.0 / 8.0) * sin(M_PI * at) / at;
                else
                        f2 = cos(M_PI * at) / f1;
		f3 = f2 * sinc(t);
#if 0
		logprintf(258, "p3d: i %4u t %10g at %10g f1 %10g f3 %10g\n", i, t, at, f1, f3);
#endif
                coeff[i % RXFILTOVER][i / RXFILTOVER] = f3;
        }
#endif
        if (logcheck(257)) {
                char buf[32768];
                char *cp = buf;
                for (i = 0; i < RXFILTOVER*rx->rxfiltlen; i++)
                        cp += sprintf(cp, " %g", coeff[i % RXFILTOVER][i / RXFILTOVER]);
                logprintf(257, "p3d: pulse = [ %s ];\n", buf+1);
        }
	max1 = 0;
        for (i = 0; i < RXFILTOVER; i++) {
                max2 = 0;
                for (j = 0; j < rx->rxfiltlen; j++)
                        max2 += fabs(coeff[i][j]);
                if (max2 > max1)
                        max1 = max2;
        }
        max2 = ((float)0x7fffffff / (float)0x7fff) / max1;
        for (i = 0; i < RXFILTOVER; i++) {
                f1 = 0;
                for (j = 0; j < rx->rxfiltlen; j++) {
                        rx->basebandfilter[i][j] = max2 * coeff[RXFILTOVER-1-i][j];
                        f1 += rx->basebandfilter[i][j] * rx->basebandfilter[i][j];
                }
                pulseen[i] = f1;
        }
        if (logcheck(257)) {
                char buf[512];
                char *cp = buf;
                for (i = 0; i < RXFILTOVER; i++)
                        cp += sprintf(cp, ", %6.2gdB", 10*M_LOG10E*log(pulseen[i]) - 10*M_LOG10E*log(32768.0 * (1<<16)));
                logprintf(257, "p3d: pulse energies: %s\n", buf+2);
        }
}

static cplxint_t calc_rxfilter(const int16_t *val, const struct rxfilter *filter, unsigned int filtlen, unsigned int phase)
{
	const int16_t *re, *im;
	cplxint_t r;

	re = filter->re[RXFILTFIDX(phase)];
	im = filter->im[RXFILTFIDX(phase)];
	val += RXFILTFSAMP(phase);
	r.re = simdfir16(val, re, filtlen) >> 15;
	r.im = simdfir16(val, im, filtlen) >> 15;
	return r;
}

static cplxint_t calc_baseband_rxfilter(const struct rxstate *rx, const int16_t *re, const int16_t *im,
					unsigned int phase)
{
	unsigned int ph1, ph2;
	cplxint_t r;

	ph1 = RXFILTFIDX(phase);
	ph2 = RXFILTFSAMP(phase);
	r.re = simdfir16(re + ph2, rx->basebandfilter[ph1], rx->rxfiltlen) >> 15;
	r.im = simdfir16(im + ph2, rx->basebandfilter[ph1], rx->rxfiltlen) >> 15;
	return r;
}

/* ---------------------------------------------------------------------- */

#define CRXBLOCKSIZE 1024
#define SYNCTIMEOUT  (5*SYMRATE)

static unsigned int contrx(struct rxstate *rx, unsigned int phase, unsigned int phinc, unsigned int freqinc)
{
	unsigned int bufphase = (phase - (10 << 16)) & ~0xffff, freq = 0, phearlylate = phinc >> 3, phcomp = phinc >> 7;
	unsigned int sbufptr = 0, i, me, ml, shreg = 0, rxbufptr = (512+2)*8;
	u_int16_t crc = 0xffff;
	int16_t samp_re[CRXBLOCKSIZE+RXFILTLEN], samp_im[CRXBLOCKSIZE+RXFILTLEN];
	cplxint_t sbuf[32], lasts, news, earlys, lates, r1;
	unsigned char rxbuf[512+2];

	audioread(rx->chan, &samp_im[0], RXFILTLEN, bufphase >> 16);
	for (i = 0; i < RXFILTLEN; i++) {
		r1.re = samp_im[i] * COS(freq);
		r1.im = - samp_im[i] * SIN(freq);
		freq += freqinc;
		samp_re[i] = r1.re >> 15;
		samp_im[i] = r1.im >> 15;
	}
	lasts.re = lasts.im = 0;
	for (;;) {
		audioread(rx->chan, &samp_im[RXFILTLEN], CRXBLOCKSIZE, (bufphase >> 16) + RXFILTLEN);
		for (i = RXFILTLEN; i < CRXBLOCKSIZE+RXFILTLEN; i++) {
			r1.re = samp_im[i] * COS(freq);
			r1.im = - samp_im[i] * SIN(freq);
			freq += freqinc;
			samp_re[i] = r1.re >> 15;
			samp_im[i] = r1.im >> 15;
		}
		while ((phase - bufphase) < ((CRXBLOCKSIZE+10) << 16)) {
			news = calc_baseband_rxfilter(rx, samp_re, samp_im, phase-bufphase);
			earlys = calc_baseband_rxfilter(rx, samp_re, samp_im, phase-phearlylate-bufphase);
			lates = calc_baseband_rxfilter(rx, samp_re, samp_im, phase+phearlylate-bufphase);
			phase += phinc;
			r1 = calc_baseband_rxfilter(rx, samp_re, samp_im, phase-bufphase);
			news.re -= r1.re;
			news.im -= r1.im;
			r1 = calc_baseband_rxfilter(rx, samp_re, samp_im, phase-phearlylate-bufphase);
			earlys.re -= r1.re;
			earlys.im -= r1.im;
			r1 = calc_baseband_rxfilter(rx, samp_re, samp_im, phase+phearlylate-bufphase);
			lates.re -= r1.re;
			lates.im -= r1.im;
			phase += phinc;
			me = earlys.re * earlys.re + earlys.im * earlys.im;
			ml = lates.re * lates.re + lates.im * lates.im;
			if (me > ml)
				phase -= phcomp;
			else
				phase += phcomp;
			r1.re = (news.re * lasts.re + news.im * lasts.im) >> 5;
			r1.im = (news.im * lasts.re - news.re * lasts.im) >> 5;
			sbuf[sbufptr] = r1;
			sbufptr = (sbufptr + 1) & 31;
			lasts = news;
			shreg <<= 1;
			shreg |= (r1.re < 0);
			if (rxbufptr >= (512+2)*8) {
				if ((shreg & 0xffffffff) == SYNCWORD) {
					crc = 0xffff;
					rxbufptr = 0;
					r1.re = r1.im = 0;
					for (i = 0; i < 32; i++) {
						if (sbuf[i].re < 0) {
							r1.re -= sbuf[i].re;
							r1.im -= sbuf[i].im;
						} else {
							r1.re += sbuf[i].re;
							r1.im += sbuf[i].im;
						}
					}
					simdpreparefpu();
					freqinc += (0x8000 / M_PI * SYMRATE) * atan2(r1.im, r1.re) / rx->srate;
					p3drxstate(rx->chan, 1, (rx->srate * freqinc + 0x8000) >> 16);
					logprintf(256, "p3ddemod: SYNC word: phase 0x%08x  finc: 0x%06x  (%d%+di)\n", phase, freqinc, r1.re, r1.im);
					continue;
				}
				rxbufptr++;
				if (rxbufptr < (512+2)*8+SYNCTIMEOUT)
					continue;
				return phase;
			}
			if ((rxbufptr & 7) == 7)
				rxbuf[rxbufptr >> 3] = shreg;
			rxbufptr++;
			if (rxbufptr < (512+2)*8)
				continue;
			crc = 0xffff;
			for (i = 0; i < (512+2); i++)
				crc = (crc << 8) ^ amsat_crc[((crc >> 8) ^ rxbuf[i]) & 0xff];
			p3dreceive(rx->chan, rxbuf, crc & 0xffff);
			p3drxstate(rx->chan, 2, (rx->srate * freqinc + 0x8000) >> 16);
		}
		memcpy(&samp_re[0], &samp_re[CRXBLOCKSIZE], RXFILTLEN * sizeof(samp_re[0]));
		memcpy(&samp_im[0], &samp_im[CRXBLOCKSIZE], RXFILTLEN * sizeof(samp_im[0]));
		bufphase += CRXBLOCKSIZE << 16;
	}
}

/* ---------------------------------------------------------------------- */

static void rxblock(struct rxstate *rx, unsigned int phase, unsigned int phinc, unsigned int freqinc)
{
	unsigned int samplen, i, j, crc;
	int16_t *samp_re, *samp_im;
	cplxint_t sbuf[(512+2)*8*2+1], r1, r2, r3;
	unsigned char rxbuf[512+2], *bp;

	samplen = ((((512+2)*8*2+1)*phinc) >> 16) + 1 + rx->rxfiltlen;
	samp_re = alloca(samplen * sizeof(samp_re[0]));
	samp_im = alloca(samplen * sizeof(samp_im[0]));
	audioread(rx->chan, &samp_im[0], samplen, phase >> 16);
	phase &= 0xffff;
	/* downconvert */
	j = 0;
	for (i = 0; i < samplen; i++) {
		r1.re = samp_im[i] * COS(j);
		r1.im = - samp_im[i] * SIN(j);
		j += freqinc;
		samp_re[i] = r1.re >> 15;
		samp_im[i] = r1.im >> 15;
	}
	/* baseband filtering */
	for (i = 0; i < (512+2)*8*2+1; i++) {
		sbuf[i] = calc_baseband_rxfilter(rx, samp_re, samp_im, phase);
		phase += phinc;
	}
	/* "Manchester" PSK decode */
	memset(rxbuf, 0, sizeof(rxbuf));
	bp = rxbuf;
	crc = 0xffff;
	for (i = 0; i < (512+2)*8; i++) {
		r1.re = sbuf[2*i].re - sbuf[2*i+1].re;
		r1.im = sbuf[2*i].im - sbuf[2*i+1].im;
		r2.re = sbuf[2*i+2].re - sbuf[2*i+3].re;
		r2.im = sbuf[2*i+2].im - sbuf[2*i+3].im;
		r3.re = (r2.re * r1.re + r2.im * r1.im) >> 15;
		r3.im = (r2.im * r1.re - r2.re * r1.im) >> 15;
		*bp <<= 1;
		*bp |= (r3.re < 0);
		if ((i & 7) == 7)
			bp++;
		crc <<= 1;
		crc |= ((crc >> 16) ^ (r3.re < 0)) & 1;
		if (crc & 1)
			crc ^= (1 << 5) | (1 << 12);
	}
	p3dreceive(rx->chan, rxbuf, crc & 0xffff);
}

/* ---------------------------------------------------------------------- */

static unsigned int finesync_corr(struct rxstate *rx, unsigned int phase, unsigned int phsamp, unsigned int phinc,
				  int16_t *samp_re, int16_t *samp_im, cplxint_t *freqr)
{
	unsigned int ph = phase - phsamp, i, j;
	cplxint_t sbuf[33*2], r1, r2, r3, r4;

	for (i = 0; i < 33*2; i++) {
		sbuf[i] = calc_baseband_rxfilter(rx, samp_re, samp_im, ph);
		ph += phinc;
	}
	r4.re = r4.im = 0;
	j = 0;
	for (i = 0; i < 32; i++) {
		r1.re = sbuf[2*i].re - sbuf[2*i+1].re;
		r1.im = sbuf[2*i].im - sbuf[2*i+1].im;
		r2.re = sbuf[2*i+2].re - sbuf[2*i+3].re;
		r2.im = sbuf[2*i+2].im - sbuf[2*i+3].im;
		r3.re = (r2.re * r1.re + r2.im * r1.im) >> 5;
		r3.im = (r2.im * r1.re - r2.re * r1.im) >> 5;
		j <<= 1;
		j |= (r3.re < 0);
		if (j & 1) {
			r4.re -= r3.re;
			r4.im -= r3.im;
		} else {
			r4.re += r3.re;
			r4.im += r3.im;
		}
	}
	*freqr = r4;
	if ((j & 0xffffffff) != SYNCWORD) {
		logprintf(256, "p3ddemod: finesync: phase 0x%08x word 0x%08x\n", phase, j);
		return 0;
	}
	r3.re = r3.im = 0;
	j = 0;
	for (i = 0; i < 33; i++) {
		r1.re = sbuf[2*i].re - sbuf[2*i+1].re;
		r1.im = sbuf[2*i].im - sbuf[2*i+1].im;
		if (j) {
			r3.re -= r1.re;
			r3.im -= r1.im;
		} else {
			r3.re += r1.re;
			r3.im += r1.im;
		}
		if (SYNCWORD & (0x80000000 >> i))
			j = !j;
	}
	r3.re >>= 5;
	r3.im >>= 5;
	i = r3.re * r3.re + r3.im * r3.im;
	logprintf(256, "p3ddemod: finesync: phase 0x%08x val %6u  freqr %6d%+7di\n", phase, i, r4.re, r4.im);
	return i;
}

struct finesyncparams {
	unsigned int phest;
	unsigned int phinc;
	unsigned int finc;
	unsigned int maxen;
};

static struct finesyncparams fine_sync(struct rxstate *rx, const struct rxfilter *filter,
			      unsigned int phest, unsigned int freqest)
{
	unsigned int phase = phest, phsamp, phinc, finc, i, j, samplen, maxen;
	int tmg, tmgmax;
	cplxint_t r1, maxv, freqr;
	int16_t *samp, *samp_re, *samp_im;
	struct finesyncparams fsp;

	//printf("Sync Det: phest 0x%08x  freqest 0x%08x\n", phest, freqest);
	finc = ((freqest << 16) + rx->srate/2) / rx->srate;
	phinc = ((rx->srate << 16) + SYMRATE) / (2 * SYMRATE);
	samplen = (((36+2)*2*phinc) >> 16) + rx->rxfiltlen;
	phsamp = (phest - 4*phinc) & ~0xffff;
	samp = alloca(samplen * sizeof(samp[0]));
	samp_re = alloca(samplen * sizeof(samp_re[0]));
	samp_im = alloca(samplen * sizeof(samp_im[0]));
	audioread(rx->chan, &samp[0], samplen, phsamp >> 16);
	j = 0;
	for (i = 0; i < samplen; i++) {
		r1.re = samp[i] * COS(j);
		r1.im = - samp[i] * SIN(j);
		j += finc;
		samp_re[i] = r1.re >> 15;
		samp_im[i] = r1.im >> 15;
	}
#if 0
	i = (0x7b8a-(phsamp >> 16)) & 0xffff;
	if (i < samplen) {
		printf("Samples:");
		for (j = 0; j < 16; j++)
			printf(" %d%+di", samp_re[i+j], samp_im[i+j]);
		printf("\n");
	}
#endif
	maxen = 0;
	maxv.re = maxv.im = 0;
	for (tmgmax = tmg = -16; tmg <= 16; tmg++) {
		phase = phest + tmg * (phinc >> 3);
		if (((phase + 33*2*phinc - phsamp) >> 16)+rx->rxfiltlen > samplen)
			abort();
		i = finesync_corr(rx, phase, phsamp, phinc, samp_re, samp_im, &r1);
		if (i > maxen) {
			maxen = i;
			tmgmax = tmg;
			freqr = r1;
		}
	}
	phest += tmgmax * (phinc >> 3);
	simdpreparefpu();
	finc += (0x8000 / M_PI * SYMRATE) * atan2(freqr.im, freqr.re) / rx->srate;
	j = 0;
	for (i = 0; i < samplen; i++) {
		r1.re = samp[i] * COS(j);
		r1.im = - samp[i] * SIN(j);
		j += finc;
		samp_re[i] = r1.re >> 15;
		samp_im[i] = r1.im >> 15;
	}
	maxen = 0;
	maxv.re = maxv.im = 0;
	freqr.re = freqr.im = 0;
	for (tmgmax = tmg = -8; tmg <= 8; tmg++) {
		phase = phest + tmg * (phinc >> 6);
		if (((phase + 33*2*phinc - phsamp) >> 16)+rx->rxfiltlen > samplen)
			abort();
		i = finesync_corr(rx, phase, phsamp, phinc, samp_re, samp_im, &r1);
		if (i > maxen) {
			maxen = i;
			tmgmax = tmg;
			freqr = r1;
		}
	}
	phest += tmgmax * (phinc >> 6);
	simdpreparefpu();
	finc += (0x8000 / M_PI * SYMRATE) * atan2(freqr.im, freqr.re) / rx->srate;
	fsp.phest = phest;
	fsp.phinc = phinc;
	fsp.finc = finc;
	fsp.maxen = maxen;
	return fsp;
}

/* ---------------------------------------------------------------------- */

#define BLOCKSIZE    1024
#define RBUFSIZE      512
#define SAMPLESPERBIT   8

static void synchunt(struct rxstate *rx)
{
	int16_t samp[RXFILTLEN+BLOCKSIZE];
	struct {
		unsigned int phase, phinc, freq;
		struct rxfilter filter;
		unsigned int bitstr[SAMPLESPERBIT];
		cplxint_t rbuf[RBUFSIZE];
	} carrier[FNUMBER];
	cplxint_t r1, r2, r3;
	unsigned int phase = 0, phase2, phinc, syncph = 0, rptr, i, j;
	struct finesyncparams fsp;

	memset(carrier, 0, sizeof(carrier));
	for (i = 0; i < FNUMBER; i++) {
		carrier[i].freq = j = (FCENTER - (FNUMBER-1) * FSPACING / 2) + i * FSPACING;
		carrier[i].phinc = ((j << 16) + (SAMPLESPERBIT * SYMRATE / 2)) / (SAMPLESPERBIT * SYMRATE);
	       	compute_rxfilter_from_bb(rx, &carrier[i].filter, ((j << 16) + rx->srate/2) / rx->srate);
	}
	phinc = ((rx->srate << 16) + (SAMPLESPERBIT * SYMRATE / 2)) / (SAMPLESPERBIT * SYMRATE);
  restart:
	p3drxstate(rx->chan, 0, 0);
	phase2 = phase & 0xffff;
	audioread(rx->chan, &samp[0], RXFILTLEN, phase >> 16);
	for (;;) {
		audioread(rx->chan, &samp[RXFILTLEN], BLOCKSIZE, (phase >> 16) + RXFILTLEN);
		for (rptr = 0; phase2 < (BLOCKSIZE << 16); phase2 += phinc, phase += phinc) {
			for (i = 0; i < FNUMBER; i++) {
				r1 = calc_rxfilter(samp, &carrier[i].filter, rx->rxfiltlen, phase2);
				r2.re = COS(carrier[i].phase);
				r2.im = -SIN(carrier[i].phase);
				carrier[i].phase += carrier[i].phinc;
				r3.re = (r2.re * r1.re - r2.im * r1.im) >> 15;
				r3.im = (r2.im * r1.re + r2.re * r1.im) >> 15;
				carrier[i].rbuf[rptr + 2*SAMPLESPERBIT] = r3;
			}
			rptr++;
		}
		/* do differential decode */
		for (i = 0; i < rptr; i++) {
			for (j = 0; j < FNUMBER; j++) {
				r1.re = carrier[j].rbuf[i].re - carrier[j].rbuf[i+SAMPLESPERBIT/2].re;
				r1.im = carrier[j].rbuf[i].im - carrier[j].rbuf[i+SAMPLESPERBIT/2].im;
				r2.re = carrier[j].rbuf[i+SAMPLESPERBIT].re - carrier[j].rbuf[i+3*SAMPLESPERBIT/2].re;
				r2.im = carrier[j].rbuf[i+SAMPLESPERBIT].im - carrier[j].rbuf[i+3*SAMPLESPERBIT/2].im;
				r3.re = (r2.re * r1.re + r2.im * r1.im) >> 15;
				r3.im = (r2.im * r1.re - r2.re * r1.im) >> 15;
				carrier[j].bitstr[syncph] <<= 1;
				carrier[j].bitstr[syncph] |= (r3.re < 0);
				if (SYNCWORD == (carrier[j].bitstr[syncph] & 0xffffffff)) {
					//printf("\n\n====== SYNC DETECTED: phase %08x rbuf %u freq %u syncph %u\n\n\n", phase, i, j, syncph);
					fsp = fine_sync(rx, &carrier[j].filter, phase - (rptr-i+33*SAMPLESPERBIT) * phinc, carrier[j].freq);
					if (fsp.maxen > 10) {
						p3drxstate(rx->chan, 1, (rx->srate * fsp.finc + 0x8000) >> 16);
#if 0
						rxblock(rx, fsp.phest+32*2*fsp.phinc, fsp.phinc, fsp.finc);
						phase = fsp.phest + (4+512+2)*8*2*phinc;
#else
						phase = contrx(rx, fsp.phest, fsp.phinc, fsp.finc);
#endif
						goto restart;
					}
				}
#if 0
				if (!syncph)
					printf("\n");
				printf("| %c ", '0'+(r3.re < 0));
#endif
			}
			syncph++;
			if (syncph >= SAMPLESPERBIT)
				syncph = 0;
		}
		fflush(stdout);
		/* prepare for next iteration */
		phase2 -= (BLOCKSIZE << 16);
		memcpy(&samp[0], &samp[BLOCKSIZE], RXFILTLEN * sizeof(samp[0]));
		for (i = 0; i < FNUMBER; i++) {
			memcpy(&carrier[i].rbuf[0], &carrier[i].rbuf[rptr], 
			       2*SAMPLESPERBIT*sizeof(carrier[i].rbuf[0]));
		}
	}
}

static void receiver(void *__rx)
{
	struct rxstate *rx = (struct rxstate *)__rx;

	synchunt(rx);
}

static const struct modemparams demodparams[] = {
        { NULL }
        
};

static void *demodconfig(struct modemchannel *chan, unsigned int *samplerate, const char *params[])
{
        struct rxstate *s;

        if (!(s = calloc(1, sizeof(struct rxstate))))
                logprintf(MLOG_FATAL, "out of memory\n");
        s->chan = chan;
	*samplerate = 8000;
	return s;
}

static void demodinit(void *state, unsigned int samplerate, unsigned int *bitrate)
{
        struct rxstate *s = (struct rxstate *)state;
	
	s->srate = samplerate;
	*bitrate = 400;
	compute_rxfilter(s);
}

struct demodulator p3ddemodulator = {
	NULL,
	"p3d",
	demodparams,
	demodconfig,
	demodinit,
	receiver,
	free
};

/* ---------------------------------------------------------------------- */
