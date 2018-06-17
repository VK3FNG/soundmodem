#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "modem.h"

#include "complex.h"
#include "modemconfig.h"
#include "fec.h"
#include "filter.h"
#include "newqpskrx.h"
#include "tbl.h"
#include "misc.h"

/* --------------------------------------------------------------------- */

static void rxidle(void *);
static void rxtune(void *);
static void rxdata(void *);

/* --------------------------------------------------------------------- */

void init_newqpskrx(void *state)
{
	struct rxstate *s = (struct rxstate *)state;
	int i;

	/* clear dcd */
	pktsetdcd(s->chan, 0);
	if (s->mintune != 0) {
		/* switch to idle mode */
		s->rxroutine = rxidle;
		s->rxwindowfunc = ToneWindowInp;
		s->carrfreq = 0.0;
		s->acceptance = 0;
		for (i = 0; i < TuneCarriers; i++) {
			s->tunepower[i] = 0.0;
			s->tunephase[i] = 0.0;
			s->tunecorr[i].re = 0.0;
			s->tunecorr[i].im = 0.0;
		}
	} else {
		/* switch to tune mode */
		s->rxroutine = rxtune;
		s->rxwindowfunc = DataWindowInp;
		s->atsymbol = 1;
		s->acceptance = 0;
		for (i = 0; i < TuneCarriers; i++) {
			s->power_at[i] = s->tunepower[i];
			s->corr1_at[i].re = s->tunepower[i];
			s->corr1_at[i].im = 0.0;
			s->corr2_at[i].re = s->tunepower[i];
			s->corr2_at[i].im = 0.0;

			s->power_inter[i] = s->tunepower[i];
			s->corr1_inter[i].re = s->tunepower[i];
			s->corr1_inter[i].im = 0.0;
			s->corr2_inter[i].re = s->tunepower[i];
			s->corr2_inter[i].im = 0.0;
		}
	}
}

/* --------------------------------------------------------------------- */

static void putbitbatch(void *state, unsigned data)
{
	struct rxstate *s = (struct rxstate *)state;
	unsigned int i, bit;
	unsigned char buf;

	for (i = 0; i < s->fec.bitbatchlen; i++) {
		bit = (data & (1 << i)) ? 1 : 0;
		s->shreg |= bit << 9;
		if (s->shreg & 1) {
			buf = (s->shreg >> 1) & 0xff;
			pktput(s->chan, &buf, 1);
			s->shreg &= ~0xff;
			s->shreg |= 0x100;
		}
		s->shreg >>= 1;
	}
}

/* --------------------------------------------------------------------- */

static void fft(complex *in, complex *out, float *window)
{
	int i, j, k;
	int s, sep, width, top, bot;
	float tr, ti;

	/* order the samples in bit reverse order and apply window */
	for (i = 0; i < WindowLen; i++) {
		j = rbits8(i) >> (8 - WindowLenLog);
		out[j].re = in[i].re * window[i];
		out[j].im = in[i].im * window[i];
	}

	/* in-place FFT */
	sep = 1;
	for (s = 1; s <= WindowLenLog; s++) {
		width = sep;    /* butterfly width =  2^(s-1) */
		sep <<= 1;      /* butterfly separation = 2^s */
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

static void mixer(struct rxstate *s, complex *buf)
{
	complex z;
	int i;

	for (i = 0; i < HalfSymbol; i++) {
		s->carrphase += s->carrfreq;

		if (s->carrphase > M_PI)
			s->carrphase -= 2 * M_PI;
		if (s->carrphase < -M_PI)
			s->carrphase += 2 * M_PI;

		z.re = cos(s->carrphase);
		z.im = sin(s->carrphase);

		buf[i] = cmul(buf[i], z);
	}
}

/* --------------------------------------------------------------------- */

void newqpskrx(void *state, complex *in)
{
	struct rxstate *s = (struct rxstate *)state;
	complex z;
	int i, j;

	/* make room for new samples at the end of RX window */
	memmove(s->rxwin, s->rxwin + HalfSymbol, (WindowLen - HalfSymbol) * sizeof(complex));

	/* copy the new samples */
	memcpy(s->rxwin + WindowLen - HalfSymbol, in, HalfSymbol * sizeof(complex));

	/* mix the new samples with the internal NCO */
	mixer(s, s->rxwin + WindowLen - HalfSymbol);

	/* apply window function and fft */
	fft(s->rxwin, s->fftbuf, s->rxwindowfunc);

	/* select the wanted FFT bins and adjust the phases */
	s->rxphasecorr = (s->rxphasecorr + HalfSymbol) % WindowLen;
	j = FirstDataCarr;
	for (i = 0; i < DataCarriers; i++) {
		z.re =  CosTable[(j * s->rxphasecorr) % WindowLen];
		z.im = -SinTable[(j * s->rxphasecorr) % WindowLen];
		s->rxpipe[s->rxptr][i] = cmul(s->fftbuf[j], z);
		j += DataCarrSepar;
	}

	/* process the data */
	s->rxroutine(state);

	s->rxptr = (s->rxptr + 1) % RxPipeLen;
}

/* --------------------------------------------------------------------- */

static void rxidle(void *state)
{
	struct rxstate *s = (struct rxstate *)state;
	float x;
	complex z;
	int i, j, cntr;
	unsigned int prev, curr;
	char buf[256];

	curr = s->rxptr;
	prev = (curr - 1) % RxPipeLen;

	j = (FirstTuneCarr - FirstDataCarr) / DataCarrSepar;

	cntr = 0;
	for (i = 0; i < TuneCarriers; i++) {
		x = cpwr(s->rxpipe[curr][j]);
		s->tunepower[i] = avg(s->tunepower[i], x, RxAverFollow);

		z = ccor(s->rxpipe[curr][j], s->rxpipe[prev][j]);
		s->tunecorr[i].re = avg(s->tunecorr[i].re, z.re, RxAverFollow);
		s->tunecorr[i].im = avg(s->tunecorr[i].im, z.im, RxAverFollow);

		if (2 * cmod(s->tunecorr[i]) > s->tunepower[i])
			cntr++;

		j += TuneCarrSepar / DataCarrSepar;
	}

	if (cntr >= TuneCarriers - 1)
		s->acceptance++;
	else if (s->acceptance > 0)
		s->acceptance--;

	if (s->acceptance < 2 * s->mintune)
		return;

#ifdef RxAvoidPTT
	//	if (sm_getptt(state))
	//		return;
#endif

	/* ok, we have a carrier */
	pktsetdcd(s->chan, 1);

	for (i = 0; i < TuneCarriers; i++)
		s->tunephase[i] = carg(s->tunecorr[i]);

	x = phaseavg(s->tunephase, TuneCarriers);
	s->carrfreq += x * 4.0 / WindowLen;

	buf[0] = 0;
	for (i = 0; i < TuneCarriers; i++) {
		x = s->tunepower[i] / (s->tunepower[i] - cmod(s->tunecorr[i]));
		sprintf(buf + strlen(buf), "%+.1fHz/%.1fdB  ",
			s->tunephase[i] * 4.0 / WindowLen /
			(2.0 * M_PI / s->srate),
			10 * log10(x));
	}
	logprintf(MLOG_INFO, "Tune tones: %s\n", buf);

	/* switch to tune mode */
	s->rxroutine = rxtune;
	s->rxwindowfunc = DataWindowInp;
	s->atsymbol = 1;
	s->acceptance = 0;
	s->statecntr = 2 * RxTuneTimeout;
	for (i = 0; i < TuneCarriers; i++) {
		s->power_at[i] = s->tunepower[i];
		s->corr1_at[i].re = s->tunepower[i];
		s->corr1_at[i].im = 0.0;
		s->corr2_at[i].re = s->tunepower[i];
		s->corr2_at[i].im = 0.0;

		s->power_inter[i] = s->tunepower[i];
		s->corr1_inter[i].re = s->tunepower[i];
		s->corr1_inter[i].im = 0.0;
		s->corr2_inter[i].re = s->tunepower[i];
		s->corr2_inter[i].im = 0.0;
	}
}

/* --------------------------------------------------------------------- */

static void rxtune(void *state)
{
	struct rxstate *s = (struct rxstate *)state;
	int i, j, cntr;
	unsigned int prev2, prev1, curr;
	complex *cor1, *cor2, z;
	float *pwr, x;

	/* timeout only if mintune not zero */
	if (s->mintune && s->statecntr-- <= 0) {
		/* timeout waiting sync - go back to idling */
		init_newqpskrx(state);
		return;
	}
	curr = s->rxptr;
	prev1 = (curr - 1) % RxPipeLen;
	prev2 = (curr - 2) % RxPipeLen;

	s->atsymbol ^= 1;

	if (s->atsymbol) {
		pwr = s->power_at;
		cor1 = s->corr1_at;
		cor2 = s->corr2_at;
	} else {
		pwr = s->power_inter;
		cor1 = s->corr1_inter;
		cor2 = s->corr2_inter;
	}

	j = (FirstTuneCarr - FirstDataCarr) / DataCarrSepar;
	for (i = 0; i < TuneCarriers; i++) {
		x = cpwr(s->rxpipe[curr][j]);
		pwr[i] = avg(pwr[i], x, RxAverFollow - 1);

		z = ccor(s->rxpipe[curr][j], s->rxpipe[prev1][j]);
		cor1[i].re = avg(cor1[i].re, z.re, RxAverFollow - 1);
		cor1[i].im = avg(cor1[i].im, z.im, RxAverFollow - 1);

		z = ccor(s->rxpipe[curr][j], s->rxpipe[prev2][j]);
		cor2[i].re = avg(cor2[i].re, z.re, RxAverFollow - 1);
		cor2[i].im = avg(cor2[i].im, z.im, RxAverFollow - 1);

		j += TuneCarrSepar / DataCarrSepar;
	}

	if (!s->atsymbol)
		return;

	cntr = 0;
	for (i = 0; i < TuneCarriers; i++) {
		if (s->power_at[i] > s->power_inter[i]) {
			x = s->power_at[i];
			z = s->corr2_at[i];
		} else {
			x = s->power_inter[i];
			z = s->corr2_inter[i];
		}

		if (-z.re > x / 2)
			cntr++;
	}

	if (cntr >= TuneCarriers - 1)
		s->acceptance++;
	else if (s->acceptance > 0)
		s->acceptance--;

	if (s->acceptance < s->minsync)
		return;

	/* again, we have a carrier */
	pktsetdcd(s->chan, 1);

	cntr = 0;
	for (i = 0; i < TuneCarriers; i++) {
		if (s->corr2_at[i].re < s->corr2_inter[i].re)
			cntr++;
		else
			cntr--;
	}

	if (cntr < 0) {
		s->atsymbol = 0;
		pwr = s->power_inter;
		cor1 = s->corr1_inter;
		cor2 = s->corr2_inter;
	}

	for (i = 0; i < TuneCarriers; i++) {
		z.re = -cor2[i].re;
		z.im = cor1[i].re - (pwr[i] + cor2[i].re) / 2;

		s->syncdelay[i] = carg(z);

		z.re = -cor2[i].re;
		z.im = cor2[i].im;
		s->syncphase[i] = carg(z);
	}

	x = phaseavg(s->syncdelay, TuneCarriers) / M_PI;
	s->skip = (0.5 - x) * SymbolLen;

	logprintf(MLOG_INFO, "Sync: %d (%s-symbol)\n", s->skip, s->atsymbol ? "at" : "inter");

	x = phaseavg(s->syncphase, TuneCarriers);
	s->carrfreq -= x * 2.0 / WindowLen;
	logprintf(MLOG_INFO, "Preamble at: %+.2fHz\n",
		  s->carrfreq / (2.0 * M_PI / s->srate));

	/* switch to data mode */
	s->rxroutine = rxdata;
	s->atsymbol ^= 1;
	s->acceptance = DCDMaxDrop / 2;
	s->updhold = RxUpdateHold;
	s->bitbatches = 0;
	/* init deinterleaver */
	init_inlv(&s->fec);
	for (i = 0; i < DataCarriers; i++) {
		s->phesum[i] = 0.0;
		s->pheavg[i] = 0.0;
		s->dcdavg[i] = 0.0;
		s->power[i]  = 0.0;
		s->correl[i] = 0.0;
		s->fecerrors[i] = 0;
	}

	s->phemax = 0.0;
}

/* --------------------------------------------------------------------- */

static void rxdata(void *state)
{
	struct rxstate *s = (struct rxstate *)state;
	int rxword[SymbolBits], errword[SymbolBits];
	int i, j, bits, data, dcd, errs, cor1, cor2;
	unsigned int prev2, prev1, curr;
	float pherr[DataCarriers];
	complex z;
	float x;
	char buf[256];

	/* nothing to do if inter-symbol */
	if ((s->atsymbol ^= 1) == 0)
		return;

	curr = s->rxptr;
	prev1 = (curr - 1) % RxPipeLen;
	prev2 = (curr - 2) % RxPipeLen;

	for (i = 0; i < SymbolBits; i++) {
		rxword[i] = 0;
		errword[i] = 0;
	}

	/*
	 * Delay the dcd/pherror/power/sync updates for the first
	 * `RxUpdateHold´ symbols because tune phase hasn't
	 * necessarily ended yet.
	 */
	if (s->updhold)
		s->updhold--;

	for (i = 0; i < DataCarriers; i++) {
		/* get the angle and add bias */
		z = ccor(s->rxpipe[curr][i], s->rxpipe[prev2][i]);
		x = carg(z) + M_PI / PhaseLevels;

		if (x < 0)
			x += 2 * M_PI;

		/* work out the bits */
		bits = (int) (x * PhaseLevels / (2 * M_PI));
		bits &= (1 << SymbolBits) - 1;

		/* calculate phase error (`0.5´ compensates the bias) */
		pherr[i] = x - (bits + 0.5) * 2 * M_PI / PhaseLevels;

		/* flip the top bit back */
		bits ^= 1 << (SymbolBits - 1);

		/* gray decode */
		data = 0;
		for (j = 0; j < SymbolBits; j++)
			data ^= bits >> j;

		/* put the bits to rxword (top carrier first) */
		for (j = 0; j < SymbolBits; j++)
			if (data & (1 << (SymbolBits - 1 - j)))
				rxword[j] |= 1 << i;

		/* skip the rest if still holding updates */
		if (s->updhold)
			continue;

		/* update phase error power average */
		s->dcdavg[i] = avg2(s->dcdavg[i], pherr[i] * pherr[i], DCDTuneAverWeight);

		/* update phase error average */
		s->pheavg[i] = avg2(s->pheavg[i], pherr[i], DCDTuneAverWeight);

		/* update carrier power average */
		x = cpwr(s->rxpipe[curr][i]);
		s->power[i] = avg(s->power[i], x, RxDataSyncFollow);

		/* update sync correlation average */
		x = ccorI(s->rxpipe[prev1][i], s->rxpipe[curr][i]) -
		    ccorI(s->rxpipe[prev1][i], s->rxpipe[prev2][i]);
		s->correl[i] = avg(s->correl[i], x, RxDataSyncFollow);
	}

	/* feed the data to the decoder */
	for (i = 0; i < SymbolBits; i++) {
		rxword[i] = deinlv(&s->fec, rxword[i]);
		rxword[i] = fecdecode(&s->fec, rxword[i], &errword[i]);
		putbitbatch(state, rxword[i]);
	}

	/* count carriers that have small enough phase error */
	for (dcd = 0, i = 0; i < DataCarriers; i++)
		if (s->dcdavg[i] < DCDThreshold)
			dcd++;

	/* decide if this was a "good" symbol */
	if (dcd >= DataCarriers / 2) {
		/* count FEC errors */
		for (i = 0; i < SymbolBits; i++) {
			for (j = 0; j < DataCarriers; j++)
				if (errword[i] & (1 << j))
					s->fecerrors[j]++;
			s->bitbatches++;
		}
#if 0
		/* sync tracking */
		for (cor1 = cor2 = i = 0; i < DataCarriers; i++) {
			if (s->power[i] < fabs(s->correl[i]) * RxSyncCorrThres) {
				if (s->correl[i] >= 0)
					cor1++;
				else
					cor2++;
			}
		}

		if (cor1 > DataCarriers * 2 / 3)
			s->skip--;
		else if (cor2 > DataCarriers * 2 / 3)
			s->skip++;

		if (s->skip != 0) {
			for (i = 0; i < DataCarriers; i++)
				s->correl[i] /= 2.0;
			logprintf(MLOG_INFO, "Correcting sync: %+d\n", s->skip);
		}
#endif
		/* sum up phase errors */
		for (i = 0; i < DataCarriers; i++)
			s->phesum[i] += pherr[i] * pherr[i];

		/* sum up maximum possible error */
		s->phemax += M_PI * M_PI / PhaseLevels / PhaseLevels;

		/* correct frequency error */
		x = phaseavg(pherr, DataCarriers);
		s->carrfreq += RxFreqFollowWeight * x * 2.0 / WindowLen;

		/* increase acceptance */
		if (s->acceptance < DCDMaxDrop)
			s->acceptance++;
	} else {
		/* drop acceptance */
		if (s->acceptance > 0)
			s->acceptance--;
	}

	if (s->acceptance > 0)
		return;

	/* DCDMaxDrop subsequent "bad" symbols, it's gone... */
	logprintf(MLOG_INFO, "Carrier lost at: %+.2fHz\n",
		  s->carrfreq / (2.0 * M_PI / s->srate));
	errs = 0;
	buf[0] = 0;
	for (i = 0; i < DataCarriers; i++) {
		errs += s->fecerrors[i];
		sprintf(buf + strlen(buf), "%02d ", s->fecerrors[i]);
	}
	logprintf(MLOG_INFO, "FEC errors: %s: %03d / %03d\n", buf, errs, s->bitbatches);

	buf[0] = 0;
	for (i = 0; i < DataCarriers; i++) {
		sprintf(buf + strlen(buf), "%2.0f ", 10 * log10(s->phemax * 2 / s->phesum[i]));
	}
	logprintf(MLOG_INFO, "S/N ratio:  %s dB\n", buf);

	/* go back to idling */
	init_newqpskrx(state);
	return;
}

/* --------------------------------------------------------------------- */

