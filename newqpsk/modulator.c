/*****************************************************************************/

/*
 *      modulator.c  --  NEWQPSK modulator.
 *
 *      Copyright (C) 2000  Tomi Manninen, OH2BNS (tomi.manninen@hut.fi)
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
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "modem.h"
#include "modemconfig.h"
#include "fec.h"
#include "filter.h"
#include "newqpsktx.h"
#include "complex.h"
#include "tbl.h"

#include <stdlib.h>
#include <stdio.h>

/* --------------------------------------------------------------------- */

static const struct modemparams modparams[] = {
	{ "bps", "Bits/s", "Bits per second", "2500", MODEMPAR_NUMERIC, { n: { 1000, 5000, 100, 500 } } },
	{ "inlv", "Interleave", "Interleave depth", "8", MODEMPAR_NUMERIC, { n: { 0, 16, 1, 4 } } },
	{ "fec", "FEC", "FEC level", "3", MODEMPAR_NUMERIC, { n: { 0, 3, 1, 1 } } },
	{ "tunelen", "Tune length", "Tune preamble length", "32", MODEMPAR_NUMERIC, { n: { 0, 64, 1, 1 } } },
	{ "synclen", "Sync length", "Sync preamble length", "32", MODEMPAR_NUMERIC, { n: { 16, 64, 1, 1 } } },
	{ NULL }
};

#define SAMPLERATE(x)	((float)(x)*SymbolLen/DataCarriers/SymbolBits)

static void *modconfig(struct modemchannel *chan, unsigned int *samplerate, const char *params[])
{
	struct txstate *s = calloc(1, sizeof(struct txstate));

	if ((s = calloc(1, sizeof(struct txstate))) == NULL)
		logprintf(MLOG_FATAL, "out of memory\n");
	s->chan = chan;
	s->saved = -1;
	if (params[0]) {
		s->bps = strtoul(params[0], NULL, 0);
		if (s->bps < 1000)
			s->bps = 1000;
		if (s->bps > 5000)
			s->bps = 5000;
	} else
		s->bps = 2500;
	if (params[1]) {
		s->fec.inlv = strtoul(params[1], NULL, 0);
		if (s->fec.inlv < 0)
			s->fec.inlv = 0;
		if (s->fec.inlv > 16)
			s->fec.inlv = 16;
	} else
		s->fec.inlv = 8;
	if (params[2]) {
		s->fec.feclevel = strtoul(params[2], NULL, 0);
		if (s->fec.feclevel < 0)
			s->fec.feclevel = 0;
		if (s->fec.feclevel > 3)
			s->fec.feclevel = 3;
	} else
		s->fec.feclevel = 3;
	if (params[3]) {
		s->tunelen = strtoul(params[3], NULL, 0);
		if (s->tunelen < 0)
			s->tunelen = 0;
		if (s->tunelen > 64)
			s->tunelen = 64;
	} else
		s->tunelen = 32;
	if (params[4]) {
		s->synclen = strtoul(params[4], NULL, 0);
		if (s->synclen < 16)
			s->synclen = 16;
		if (s->synclen > 64)
			s->synclen = 64;
	} else
		s->synclen = 32;
	*samplerate = (int) (3.0 * SAMPLERATE(s->bps) + 0.5);
	return s;
}

static void modinit(void *state, unsigned int samplerate)
{
	struct txstate *s = (struct txstate *)state;
	float rate, f1, f2;

	rate = samplerate / SAMPLERATE(s->bps);
	s->bufsize = rate * SymbolLen + 16;
	f1 = 0.1;
	f2 = 0.9;
	init_tbl();
	init_filter(&s->filt, rate, f1, f2);
	init_fec(&s->fec);
	init_newqpsktx(state);
}

static void modmodulate(void *state, unsigned int txdelay)
{
	struct txstate *s = (struct txstate *)state;
	int16_t *samples;
	complex *cbuf;
	unsigned char buf;
	int n, i;

	/* check if there is data to be transmitted... */
	if (!pktget(s->chan, &buf, 1))
		return;

	/* ...and save it */
	s->saved = buf;

	samples = alloca(s->bufsize * sizeof(int16_t));
	cbuf = alloca(s->bufsize * sizeof(complex));

	s->txdone = 0;
	while (!s->txdone) {
		n = newqpsktx(state, cbuf);
		for (i = 0; i < n; i++)
			samples[i] = (cbuf[i].re + cbuf[i].im) * 32768.0;
		audiowrite(s->chan, samples, n);
	}
}

/* --------------------------------------------------------------------- */

struct modulator newqpskmodulator = {
        NULL,
        "newqpsk",
        modparams,
        modconfig,
        modinit,
        modmodulate,
        free
};

/* --------------------------------------------------------------------- */
