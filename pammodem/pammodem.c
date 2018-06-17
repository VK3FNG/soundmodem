/*****************************************************************************/

/*
 *      pammodem.c  --  PAM modem.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "pam.h"

#include "pamtbl.h"

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------- */

struct txstate {
        struct modemchannel *chan;
	unsigned int txphinc;
	unsigned int txphase;
	unsigned int filter;
};

static void txsendbits(struct txstate *tx, unsigned int bits, unsigned int nrbits)
{
	int16_t sbuf[(32*SAMPLERATE+BITRATE-1)/BITRATE];
	int16_t *sptr = sbuf;
	unsigned int i, j;
	int sum;
	const int *coeff;

	while (nrbits > 0) {
		if (tx->txphase >= 0x10000) {
			tx->txphase &= 0xffff;
			tx->filter <<= 1;
			tx->filter |= bits & 1;
			bits >>= 1;
			nrbits--;
		}
		coeff = txfilter[TXFILTFIDX(tx->txphase)];
		for (j = tx->filter, sum = i = 0; i < TXFILTLEN; i++, j >>= 1, coeff++)
			if (j & 1)
				sum += *coeff;
			else
				sum -= *coeff;
		//sum -= *coeff ^ (-(j & 1));
		*sptr++ = sum;
		tx->txphase += tx->txphinc;
	}
	audiowrite(tx->chan, sbuf, sptr - sbuf);
}

static void txsendtrain(struct txstate *tx)
{
	const unsigned char *b = trainsymbmap;
	unsigned int cnt = TRAINBITS;

	while (cnt > 8) {
		txsendbits(tx, *b++, 8);
		cnt -= 8;
	}
	txsendbits(tx, *b, cnt);
}

static int txsenddata(struct txstate *tx)
{
	unsigned char buf[DATABYTES];
	unsigned int i;
	
	if (!pktget(tx->chan, buf, DATABYTES))
		return -1;
	for (i = 0; i < DATABYTES; i++)
		txsendbits(tx, buf[i], 8);
	txsendtrain(tx);
	return 0;
}

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

	s->txphinc = ((BITRATE << 16) + samplerate / 2) / samplerate;
}

static void modmodulate(void *state, unsigned int txdelay)
{
        struct txstate *s = (struct txstate *)state;

	txsendtrain(s);
	while (!txsenddata(s));
}

struct modulator pammodulator = {
        NULL,
        "pam",
        modparams,
        modconfig,
        modinit,
        modmodulate,
	free
};

/* ---------------------------------------------------------------------- */

#define OVERLAP   128
#define SBUFSIZE  512

struct rxstate {
        struct modemchannel *chan;
	u_int16_t stime;
	int16_t rxbuf[SBUFSIZE];
	unsigned int rxphase;
	unsigned int rxphaseinc;
	unsigned int rxptr;
};

extern inline int rxgsfir(const int16_t *buf, const int *coeff)
{
	unsigned int i;
	int s;

	for (s = 0, i = 0; i < RXFILTLEN; i++)
		s += (*buf--) * (*coeff++);
	return s >> 16;
}

static void rxgetsamples(struct rxstate *rx, int16_t *samples, unsigned int nr, unsigned int tspaced)
{
	unsigned int reqph = (nr * rx->rxphaseinc) << tspaced;
	unsigned int endph = rx->rxphase + reqph;
	unsigned int phptr = RXFILTFSAMP(rx->rxphase);
	unsigned int endptr;

	if ((endph >= (SBUFSIZE << 16)) && (phptr > OVERLAP)) {
		phptr -= OVERLAP;
		rx->rxptr -= phptr;
		memmove(rx->rxbuf, rx->rxbuf + phptr, rx->rxptr * sizeof(rx->rxbuf[0]));
		rx->rxphase -= phptr << 16;
		endph = rx->rxphase + reqph;
	}
	if (endph >= (SBUFSIZE << 16))
		logprintf(MLOG_FATAL, "rxgetsamples: too many samples requested\n");
	endptr = RXFILTFSAMP(endph) + 1;
	if (endptr > rx->rxptr) {
		audioread(rx->chan, rx->rxbuf + rx->rxptr, endptr - rx->rxptr, rx->stime);
		rx->stime += endptr - rx->rxptr;
		rx->rxptr = endptr;
	}
	for (; nr > 0; nr--, samples++) {
		rx->rxphase += rx->rxphaseinc << tspaced;
		*samples = rxgsfir(rx->rxbuf + RXFILTFSAMP(rx->rxphase), rxfilter[RXFILTFIDX(rx->rxphase)]);
	}
}

static void rxrewindsamples(struct rxstate *rx, unsigned int nr, unsigned int tspaced)
{
	unsigned int ph = (nr * rx->rxphaseinc) << tspaced;

	if (ph > rx->rxphase)
		logprintf(MLOG_FATAL, "rxrewindsamples: too many samples requested\n");
	rx->rxphase -= ph;
}

extern inline int calcsync(int *toten, int *corren, int16_t *samples)
{
	const int *tr = trainsyms;
	unsigned int i;
	int acc1, acc2;

	for (acc1 = acc2 = 0, i = 0; i < TRAINBITS; i++, samples += 2, tr++) {
		acc1 += (*tr) * (*samples);
		acc2 += ((*samples) * (*samples)) >> 10;
	}
	acc1 >>= 5;
	acc1 *= acc1;
	acc1 /= TRAINBITS;
	if (toten)
		*toten = acc2;
	if (corren)
		*corren = acc1;
	//fprintf(stderr, "Sync energy %d correlation %d\n", acc2, acc1);
	if (acc2 < 16*TRAINBITS || acc1*3 < acc2)
		return 0;
	logprintf(256, "Sync found, energy %d correlation %d\n", acc2, acc1);
	return 1;
}

static void synchunt(struct rxstate *rx, int *channel, int *chenergy, unsigned int dcdcnt)
{
	int16_t syncbuf[4*TRAINBITS+1];
	unsigned int i, j;
	int trseq[OBSTRAINBITS], sum, en;
	int toten1, toten2, syncen1, syncen2;
	const int *p1, *p2;

	rxgetsamples(rx, syncbuf, 4*TRAINBITS+1, 0);
	for (;;) {
		for (i = 0; i < 2*TRAINBITS; i++) {
			if (calcsync(&toten1, &syncen1, syncbuf+i)) {
				if (!calcsync(&toten2, &syncen2, syncbuf+i+1) || syncen2 < syncen1) {
					rxrewindsamples(rx, 4*TRAINBITS-2*(OBSTRAINBITS-1)-(CHANNELLEN & ~1)-i, 0);
					for (j = 0; j < OBSTRAINBITS; j++)
						trseq[j] = syncbuf[i+2*j+(CHANNELLEN & ~1)];
					goto syncfound;
				}
			}
			if (dcdcnt > 0) {
				dcdcnt--;
				if (!dcdcnt)
					pktsetdcd(rx->chan, 0);
			}
		}
		memmove(syncbuf, syncbuf+2*TRAINBITS, (2*TRAINBITS+1)*sizeof(syncbuf[0]));
		rxgetsamples(rx, syncbuf+(2*TRAINBITS+1), 2*TRAINBITS, 0);
	}
  syncfound:
	for (p1 = trainmat, en = 0, i = 0; i < CHANNELLEN; i++) {
		for (sum = 0, p2 = trseq, j = 0; j < OBSTRAINBITS; j++)
			sum += (*p1++) * (*p2++);
		sum >>= 16;
		channel[i] = sum;
		en += sum * sum;
	}
	*chenergy = en;
	pktsetdcd(rx->chan, 1);
	if (logcheck(256)) {
		char buf[512];
		char *sptr;
		sptr = buf + sprintf(buf, "Sync found, chenergy %d, dcd %u, channel", en, dcdcnt);
		for (i = 0; i < CHANNELLEN; i++)
			sptr += sprintf(sptr, " %d", channel[i]);
		logprintf(256, "%s\n", buf);
	}
}

/*
 * Maximum Likelyhood Sequence Estimation
 */

#define MLSEENERGYSHIFT  6
#define MLSENRNODES      (1<<((CHANNELLEN)-1))
#define MLSEHALFNRNODES  ((MLSENRNODES)>>1)

struct mlsenode {
	unsigned int metric;
};

static inline unsigned int mlsemetric(int diff)
{
	return (diff * diff) >> MLSEENERGYSHIFT;
}

static void mlsetrellis(struct mlsenode *nptr1, struct mlsenode *nptr2, int16_t *metrictab, unsigned char *backtrack, int val)
{
	unsigned int x0, x1, y0, y1, xm00, xm01, xm10, xm11;
        unsigned int m00, m01, m10, m11;

        for (x0 = 0; x0 < MLSEHALFNRNODES; x0++) {
                x1 = x0 + MLSEHALFNRNODES;
                y0 = x0 << 1;
                y1 = y0 | 1;
                xm00 = x0 << 1;
                xm10 = x1 << 1;
                xm01 = xm00 | 1;
                xm11 = xm10 | 1;
                m00 = nptr1[x0].metric + mlsemetric(val - metrictab[xm00]);
                m01 = nptr1[x0].metric + mlsemetric(val - metrictab[xm01]);
                m10 = nptr1[x1].metric + mlsemetric(val - metrictab[xm10]);
                m11 = nptr1[x1].metric + mlsemetric(val - metrictab[xm11]);
#if 0
		printf("x0 %02x x1 %02x y0 %02x y1 %02x m00 %10u m10 %10u m01 %10u m11 %10u\n",
		       x0, x1, y0, y1, m00, m10, m01, m11);
#endif
                if (m00 < m10) {
                        nptr2[y0].metric = m00;
			backtrack[y0] = x0;
                } else {
                        nptr2[y0].metric = m10;
			backtrack[y0] = x1;
                }
                if (m01 < m11) {
                        nptr2[y1].metric = m01;
			backtrack[y1] = x0;
                } else {
                        nptr2[y1].metric = m11;
			backtrack[y1] = x1;
                }
	}
}

static unsigned int mlsebacktrack(unsigned char *byte, unsigned int nrbits, unsigned int startnode, unsigned char *backtrack)
{
	unsigned int bshreg = 0;

	for (; nrbits > 0; nrbits--, backtrack -= MLSENRNODES) {
		bshreg <<= 1;
		bshreg |= startnode & 1;
		startnode = backtrack[startnode];
	}
	if (byte)
		*byte = bshreg;
	return startnode;
}

static void mlseblock(struct rxstate *rx, int *channel)
{
	unsigned int energy = 0;
	struct mlsenode nodes[2*MLSENRNODES], *nptr1, *nptr2, *nptr3;
	int16_t metrictab[1<<CHANNELLEN];
	unsigned char backtrack[(DATABITS+CHANNELLEN-1)*MLSENRNODES];
	unsigned char *btptr = backtrack;
	unsigned char msg[DATABYTES];
	int16_t samp[8];
	unsigned int i, j, k;
	int sum;

	/* initialize MLSE state */
	for (i = 0; i < (1<<CHANNELLEN); i++) {
		for (sum = 0, j = 0; j < CHANNELLEN; j++)
			if (i & (1 << j))
				sum += channel[CHANNELLEN-1-j];
			else
				sum -= channel[CHANNELLEN-1-j];
		metrictab[i] = sum;
	}
	nptr1 = nodes;
	nptr2 = nodes + MLSENRNODES;
	for (i = 0; i < MLSENRNODES; i++)
		nptr1[i].metric = UINT_MAX >> 1;
	nptr1[MLSEROOTNODE].metric = 0;
	/* MLSE decoder loop */
	for (i = DATABITS+CHANNELLEN-1; i > 0;) {
		j = i;
		if (j > 8)
			j = 8;
		i -= j;
		rxgetsamples(rx, samp, j, 1);
		for (k = 0; k < j; k++) {
			energy += mlsemetric(samp[k]);
			mlsetrellis(nptr1, nptr2, metrictab, btptr, samp[k]);
			nptr3 = nptr1;
			nptr1 = nptr2;
			nptr2 = nptr3;
			btptr += MLSENRNODES;
		}
	}
	rxrewindsamples(rx, CHANNELLEN, 1);
	/* backtracking */
	btptr -= MLSENRNODES;
	k = mlsebacktrack(msg, (CHANNELLEN-1), MLSETOORNODE, btptr);
	btptr -= (CHANNELLEN-1) * MLSENRNODES;
	for (i = DATABYTES-1; (signed)i >= 0; i--) {
		k = mlsebacktrack(msg+i, 8, k, btptr);
		btptr -= 8 * MLSENRNODES;
	}
	if (k != MLSEROOTNODE)
		logprintf(258, "MLSE: uhoh surviving path does not end in root node 0x%x, 0x%x\n",
			  MLSEROOTNODE, k);
#if 0
	j = 0;
	for (i = 1; i < MLSENRNODES; i++)
		if (nptr1[i].metric < nptr1[j].metric)
			j = i;
	if (j != MLSETOORNODE)
		logprintf(258, "MLSE: uhoh best metric not at toor node 0x%x, 0x%x (en %u, %u)\n", 
			MLSETOORNODE, j, nptr1[MLSETOORNODE].metric, nptr1[j].metric);
#endif
	pktput(rx->chan, msg, DATABYTES);
	logprintf(257, "MLSE: signal energy %u, error energy %u  S/(N+D) %5.1fdB\n", 
		  energy, nptr1[MLSETOORNODE].metric,
		-10*log10(nptr1[MLSETOORNODE].metric / (double)(energy ? energy : 1)));
}

static void demoddemodulate(void *state)
{
	struct rxstate *rx = state;

	rx->stime = audiocurtime(rx->chan);
	for (;;) {
		int chan[CHANNELLEN], en;
		synchunt(rx, chan, &en, 20);
		mlseblock(rx, chan);
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
	
	s->rxphaseinc = ((samplerate << 16) + BITRATE) / (2 * BITRATE);
	s->rxphase = OVERLAP << 16;
	s->rxptr = OVERLAP;
	*bitrate = 9600;
}

struct demodulator pamdemodulator = {
	NULL,
	"pam",
	demodparams,
	demodconfig,
	demodinit,
	demoddemodulate,
	free
};

/* ---------------------------------------------------------------------- */
