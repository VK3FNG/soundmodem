/*****************************************************************************/

/*
 *      pskmodem.c  --  PSK modem.
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
#include "psk.h"
#include "psktbl.h"
#include "simd.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------- */

struct txstate {
        struct modemchannel *chan;
	unsigned int txphinc;
	unsigned int txphase;
	unsigned int carphase;
	unsigned int carphinc;
	cplxshort_t filter[TXFILTLEN];
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
	*samplerate = SAMPLERATE;
	return s;
}

static void modinit(void *state, unsigned int samplerate)
{
        struct txstate *s = (struct txstate *)state;
	
	s->txphinc = ((SYMRATE << 16) + samplerate / 2) / samplerate;
	s->carphinc = ((FCARRIER << 16) + samplerate / 2) / samplerate;
}

static void txsendsymbols(struct txstate *tx, const unsigned char *sym, unsigned int nrsyms)
{
	int16_t sbuf[512];
	int16_t *sptr = sbuf, *eptr = sbuf + sizeof(sbuf)/sizeof(sbuf[0]);
	unsigned int i;
	int sumr, sumi;
	const int *coeff;

	while (nrsyms > 0) {
		if (tx->txphase >= 0x10000) {
			tx->txphase &= 0xffff;
			memmove(&tx->filter[1], tx->filter, sizeof(tx->filter) - sizeof(tx->filter[0]));
			tx->filter[0] = psk_symmapping[sym[0] & SYMBITMASK];
			sym++;
			nrsyms--;
		}
		coeff = txfilter[TXFILTFIDX(tx->txphase)];
		for (sumr = sumi = i = 0; i < TXFILTLEN; i++, coeff++) {
			sumr += (*coeff) * tx->filter[i].re;
			sumi += (*coeff) * tx->filter[i].im;
		}
		sumr >>= 15;
		sumi >>= 15;
		*sptr++ = (sumr * COS(tx->carphase) - sumi * SIN(tx->carphase)) >> 15;
		tx->carphase += tx->carphinc;
		tx->txphase += tx->txphinc;
		if (sptr >= eptr) {
			audiowrite(tx->chan, sbuf, sptr - sbuf);
			sptr = sbuf;
		}
	}
	audiowrite(tx->chan, sbuf, sptr - sbuf);
}

static void txsendtrain(struct txstate *tx)
{
	txsendsymbols(tx, trainsyms, TRAINSYMS);
}

static int txsenddata(struct txstate *tx)
{
	unsigned char buf[DATABYTES+1];
	unsigned char sym[DATASYMS];
	unsigned int i, j, k;
	unsigned char *bp = buf;

	if (!pktget(tx->chan, buf, DATABYTES))
		return -1;
	buf[DATABYTES] = 0;
	for (k = j = i = 0; i < DATASYMS; i++) {
		if (j < SYMBITS) {
			k |= (*bp++) << j;
			j += 8;
		}
		sym[i] = k & SYMBITMASK;
		logprintf(260, "txsymbol[%2u] = %u  %d%+di\n", i, sym[i], psk_symmapping[sym[i]].re, psk_symmapping[sym[i]].im);
		k >>= SYMBITS;
		j -= SYMBITS;
	}
	txsendsymbols(tx, sym, DATASYMS);
	txsendtrain(tx);
	return 0;
}

static void modmodulate(void *state, unsigned int txdelay)
{
	struct txstate *tx = (struct txstate *)state;
	unsigned char txddata[DATASYMS];
	unsigned int i, j, k;

	if (txdelay > 0) {
		for (i = j = k = 0; i < DATASYMS; i++) {
			if (j < SYMBITS) {
				k |= 0x7e << j;
				j += 8;
			}
			txddata[i] = k & SYMBITMASK;
			k >>= SYMBITS;
			j -= SYMBITS;
		}
	}
	while (txdelay > 0) {
		txsendtrain(tx);
		txsendsymbols(tx, txddata, DATASYMS);
		if (txdelay > ((TRAINSYMS+DATASYMS)*1000/SYMRATE))
			txdelay -= ((TRAINSYMS+DATASYMS)*1000/SYMRATE);
		else
			txdelay = 0;
	}
	txsendtrain(tx);
	while (!txsenddata(tx));
}

struct modulator pskmodulator = {
        NULL,
        "psk",
        modparams,
        modconfig,
        modinit,
        modmodulate,
	free
};

/* ---------------------------------------------------------------------- */

static inline int rxfilter_real(const int16_t *val, unsigned int phase)
{
	const int *coeff = rxfilter_re[RXFILTFIDX(phase)];
	unsigned int i;
	int s = 0;

	val += RXFILTFSAMP(phase);
	for (i = 0; i < RXFILTLEN; i++, val++, coeff++)
		s += (*val) * (*coeff);
	return s >> 16;
}

static inline int rxfilter_imag(const int16_t *val, unsigned int phase)
{
	const int *coeff = rxfilter_im[RXFILTFIDX(phase)];
	unsigned int i;
	int s = 0;

	val += RXFILTFSAMP(phase);
	for (i = 0; i < RXFILTLEN; i++, val++, coeff++)
		s += (*val) * (*coeff);
	return s >> 16;
}

/* ---------------------------------------------------------------------- */

#define RXCARPHASEINC       (((FCARRIER << 16) + SYMRATE / 2) / SYMRATE)

struct rxstate {
        struct modemchannel *chan;
	unsigned int srate;
};

static void rxread(struct rxstate *rx, cplxshort_t *ptr, unsigned int nr, unsigned int phase, unsigned int phaseinc)
{
	int16_t *samples;
	unsigned int totsamp = ((nr * phaseinc) >> 16) + RXFILTLEN;

	samples = alloca(totsamp * sizeof(samples[0]));
	audioread(rx->chan, samples, totsamp, phase >> 16);
	phase &= 0xffff;
	for (; nr > 0; nr--, ptr++, phase += phaseinc) {
		ptr->re = rxfilter_real(samples, phase);
		ptr->im = rxfilter_imag(samples, phase);
	}
}

static void rxrotate(cplxshort_t *ptr, unsigned int nr, unsigned int carphase, unsigned int carphaseinc)
{
	int vr, vi, cr, ci;

	for (; nr > 0; nr--, ptr++, carphase += carphaseinc) {
		vr = ptr->re;
		vi = ptr->im;
		cr = COS(carphase);
		ci = SIN(carphase);
		ptr->re = (vr * cr + vi * ci) >> 15;
		ptr->im = (vi * cr - vr * ci) >> 15;
	}
}

extern inline int calcsync(int *toten, int *corren, cplxshort_t *samples)
{
	const cplxshort_t *tr = traincorrrotated;
	unsigned int i;
	int acc1r, acc1i, acc2;

	for (acc1r = acc1i = acc2 = 0, i = 0; i < TRAINSYMS; i++, samples += 2, tr++) {
		acc1r += ((tr->re) * (samples->re) - (tr->im) * (samples->im)) >> 5;
		acc1i += ((tr->re) * (samples->im) + (tr->im) * (samples->re)) >> 5;
		acc2 += ((samples->re) * (samples->re) + (samples->im) * (samples->im)) >> 10;
	}
	acc1r >>= 15;
	acc1i >>= 15;
	acc1r = acc1r * acc1r + acc1i * acc1i;
	acc1r /= TRAINSYMS;
	if (toten)
		*toten = acc2;
	if (corren)
		*corren = acc1r;
	logprintf(258, "Sync energy %d correlation %d\n", acc2, acc1r);
	if (acc2 < 16*TRAINSYMS || acc1r*2 < acc2)
		return 0;
	logprintf(257, "Sync found, energy %d correlation %d\n", acc2, acc1r);
	return 1;
}

/*
 * searches for a training sequence; returns the phase (time) of the training sequence start.
 */

static unsigned int synchunt(struct rxstate *rx, unsigned int phase, unsigned int phaseinc, unsigned int dcdcnt)
{
	cplxshort_t syncbuf[4*TRAINSYMS+1];
	cplxshort_t syncbuf2[2*TRAINSYMS];
	unsigned int phaseblkinc;
	int toten1, toten2, toten3, syncen1, syncen2, syncen3;
	unsigned int i;

	phaseinc >>= 1;
	phaseblkinc = phaseinc * (2 * TRAINSYMS);
	rxread(rx, syncbuf, 4*TRAINSYMS+1, phase, phaseinc);
	for (;;) {
		for (i = 0; i < 2*TRAINSYMS; i++) {
			if (calcsync(&toten1, &syncen1, syncbuf+i)) {
				if (!calcsync(&toten2, &syncen2, syncbuf+i+1) || syncen2 < syncen1) {
					phase += i * phaseinc;
					rxread(rx, syncbuf2, 2*TRAINSYMS, phase - (phaseinc >> 1), phaseinc);
					calcsync(&toten2, &syncen2, syncbuf2);
					calcsync(&toten3, &syncen3, syncbuf2+1);
					if (syncen2 > syncen1 && syncen2 > syncen3)
						phase -= (phaseinc >> 1);
					else if (syncen3 > syncen1 && syncen3 > syncen2)
						phase += (phaseinc >> 1);
					logprintf(256, "Sync found, dcd %u\n", dcdcnt);
					return phase;
				}
			}
			if (dcdcnt > 0) {
				dcdcnt--;
				if (!dcdcnt)
					pktsetdcd(rx->chan, 0);
			}
		}
		phase += phaseblkinc;
		memmove(syncbuf, syncbuf+2*TRAINSYMS, (2*TRAINSYMS+1)*sizeof(syncbuf[0]));
		rxread(rx, syncbuf+(2*TRAINSYMS+1), 2*TRAINSYMS, phase + phaseblkinc, phaseinc);
	}
}

static void calcchannel(struct rxstate *rx, unsigned int phase, unsigned int phaseinc, cplxshort_t *channel, int *chenergy)
{
	cplxshort_t trseq[OBSTRAINSYMS];
	const cplxshort_t *p1, *p2;
	unsigned int i, j;
	int sumr, sumi, en;

	rxread(rx, trseq, OBSTRAINSYMS, phase + (CHANNELLEN/2) * phaseinc, phaseinc);
	for (p1 = trainmatrotated, en = 0, i = 0; i < CHANNELLEN; i++) {
		for (sumr = sumi = 0, p2 = trseq, j = 0; j < OBSTRAINSYMS; j++, p1++, p2++) {
			sumr += (p1->re) * (p2->re) - (p1->im) * (p2->im);
			sumi += (p1->re) * (p2->im) + (p1->im) * (p2->re);
		}
		sumr >>= 16;
		sumi >>= 16;
		channel[i].re = sumr;
		channel[i].im = sumi;
		en += sumr * sumr + sumi * sumi;
	}
	*chenergy = en;
	if (logcheck(256)) {
		char buf[512], *bp = buf;
		int r;
		r = snprintf(bp, buf+sizeof(buf)-bp, "Sync found, chenergy %d, channel", en);
		if (r > 0)
			bp += r;
		for (i = 0; i < CHANNELLEN; i++) {
			r = snprintf(bp, buf+sizeof(buf)-bp, " %d%+di", channel[i].re, channel[i].im);
			if (r > 0)
				bp += r;
		}
		logprintf(256, "%s\n", buf);
	}
}

/* ---------------------------------------------------------------------- */

static unsigned int mlsebacktrack(unsigned char *syms, unsigned int nrsyms, unsigned int startnode, unsigned short *backtrack)
{
	for (; nrsyms > 0; nrsyms--, syms--, backtrack -= MLSENODES) {
		if (syms)
			*syms = startnode & SYMBITMASK;
		startnode = backtrack[startnode];
	}
	return startnode;
}

static inline void symtochar(unsigned char *sym, unsigned char *msg)
{
	unsigned int i, j, k;

	for (i = j = k = 0; i < DATABYTES;) {
		k |= sym[0] << j;
		sym++;
		j += SYMBITS;
		if (j >= 8) {
			msg[0] = k;
			msg++;
			k >>= 8;
			j -= 8;
			i++;
		}
	}
}

/* ---------------------------------------------------------------------- */

static void mlseblock(struct rxstate *rx, unsigned int phase, unsigned int phaseinc, cplxshort_t *channel)
{
	unsigned int *nptr1, *nptr2, *nptr3;
	unsigned short *btptr;
	unsigned int i, j, k, carphase = 0, energy = 0;
	cplxshort_t samp[8];
	unsigned int nodes[2*MLSENODES];
	metrictab_t metrictab;
	unsigned short backtrack[(DATASYMS+CHANNELLEN-1)*MLSENODES];
	unsigned char msg[DATABYTES];
	unsigned char sym[DATASYMS+CHANNELLEN-1];

	/* initialize MLSE state */
	pskmlse_initmetric(channel, &metrictab);
	btptr = backtrack;
	nptr1 = nodes;
	nptr2 = nodes + MLSENODES;
	for (i = 0; i < MLSENODES; i++)
		nptr1[i] = UINT_MAX >> 2;
	nptr1[MLSEROOTNODE] = 0;
	/* MLSE decoder loop */
	for (i = DATASYMS+CHANNELLEN-1; i > 0;) {
		j = i;
		if (j > 8)
			j = 8;
		i -= j;
		rxread(rx, samp, j, phase, phaseinc);
		rxrotate(samp, j, carphase, RXCARPHASEINC);
		phase += j * phaseinc;
		carphase += j * RXCARPHASEINC;
		for (k = 0; k < j; k++) {
			logprintf(260, "RxSymbol: %d%+di\n", samp[k].re, samp[k].im);
			energy += (samp[k].re * samp[k].re + samp[k].im * samp[k].im) >> MLSEENERGYSH;
			pskmlse_trellis(nptr1, nptr2, &metrictab, btptr, samp[k].re, samp[k].im);
			nptr3 = nptr1;
			nptr1 = nptr2;
			nptr2 = nptr3;
			btptr += MLSENODES;
		}
	}
	/* backtracking */
	btptr -= MLSENODES;
	k = mlsebacktrack(&sym[DATASYMS+CHANNELLEN-2], DATASYMS+CHANNELLEN-1, MLSETOORNODE, btptr);
	symtochar(sym, msg);
	if (k != MLSEROOTNODE)
		logprintf(1, "MLSE: uhoh surviving path does not end in root node 0x%x, 0x%x\n", MLSEROOTNODE, k);
#if 0
	j = 0;
	for (i = 1; i < MLSENODES; i++)
		if (nptr1[i] < nptr1[j])
			j = i;
	if (j != MLSETOORNODE)
		logprintf(1, "MLSE: uhoh best metric not at toor node 0x%x, 0x%x (en %u, %u)\n", 
			  MLSETOORNODE, j, nptr1[MLSETOORNODE], nptr1[j]);
#endif
	pktput(rx->chan, msg, DATABYTES);
	k = nptr1[MLSETOORNODE];
#if defined(USEVIS)
	if (checksimd())
		k <<= 2;
#endif /* USEVIS */
	logprintf(1, "MLSE: signal energy %u, error energy %u  S/(N+D) %5.1fdB\n",
		  energy, k, -10*log10(k / (double)(energy ? energy : 1)));
}

static void receiver(void *__rx)
{
	struct rxstate *rx = (struct rxstate *)__rx;
	cplxshort_t chan[CHANNELLEN];
	unsigned int phase, oldphase = (~0U) >> 1, phaseinc, estphaseinc;
	unsigned int changle, oldchangle = 0;
	int en, splphasediff, anglediff, totanglediff, totphasediff;

	phase = audiocurtime(rx->chan) << 16;
	phaseinc = ((rx->srate << 16) + SYMRATE / 2) / SYMRATE;
	logprintf(256, "rxphaseinc: 0x%x\n", phaseinc);
	for (;;) {
		phase = synchunt(rx, phase, phaseinc, 20);
		pktsetdcd(rx->chan, 1);
		splphasediff = phase - oldphase - (TRAINSYMS+DATASYMS) * phaseinc;
		calcchannel(rx, phase, phaseinc, chan, &en);
		changle = ((1 << 15) / M_PI) * atan2(chan[CHANNELLEN/2].im, chan[CHANNELLEN/2].re);
		changle &= 0xffff;
		anglediff = (int)(int16_t)((changle-oldchangle - RXCARPHASEINC * (DATASYMS + TRAINSYMS)) & 0xffff);
		totphasediff = splphasediff - anglediff * (int)rx->srate / FCARRIER;
		totanglediff = (anglediff - splphasediff * FCARRIER / (int)rx->srate);
		estphaseinc = phaseinc;
		if (abs(splphasediff) < 0x40000) {
			estphaseinc = phaseinc - (totphasediff / (int)(DATASYMS + TRAINSYMS));
			logprintf(257, "last train phase: 0x%08x  current train phase: 0x%08x  diff: %d\n",
				  oldphase, phase, splphasediff);
			logprintf(258, "channel angle: 0x%04x  prev channel angle: 0x%04x  diff: %d\n", changle, oldchangle, anglediff);
			logprintf(257, "total angle difference: %6d  total phase difference: %8d\n", totanglediff, totphasediff);
			logprintf(257, "nominal phase inc: 0x%05x  estimated phase inc: 0x%05x  delta: %3d o/oo\n",
				  phaseinc, estphaseinc, 1000 * (int)(estphaseinc - phaseinc) / (int)phaseinc);
		}
		oldchangle = changle;
		oldphase = phase;
		phase += ((CHANNELLEN/2)+OBSTRAINSYMS) * phaseinc;
		mlseblock(rx, phase, /*est*/phaseinc, chan);
		phase += (DATASYMS-1) * phaseinc;
	}
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
	*samplerate = SAMPLERATE;
	return s;
}

static void demodinit(void *state, unsigned int samplerate, unsigned int *bitrate)
{
        struct rxstate *s = (struct rxstate *)state;
	
	s->srate = samplerate;
	*bitrate = 7600;
}

struct demodulator pskdemodulator = {
	NULL,
	"psk",
	demodparams,
	demodconfig,
	demodinit,
	receiver,
	free
};

/* ---------------------------------------------------------------------- */
