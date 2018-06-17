/*****************************************************************************/

/*
 *      pktberr.c  --  Simple "packet" generator that counts bit errors.
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
 */

/*****************************************************************************/

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "soundio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* ---------------------------------------------------------------------- */

extern void iosetsnr(struct state *state, float snr);

#define TXDELAYBITS  5000
#define TXTAILBITS   5000

static struct simstate {
	unsigned int terminate;
	float snr, snrend, snrstep;
	unsigned int nrbits, txcnt, rxcnt, rxerr;
	unsigned int rxshreg, txshreg;
} simstate = {
	0, 30, -2, 6, 100000, 0, 0, 0, 0, 1
};

static RETSIGTYPE sigterm()
{
	simstate.terminate = 1;
}

struct modemparams pktmkissparams[] = {
	{ NULL }
};

struct modemparams pktkissparams[] = {
	{ "snrstart", "SNR Start", "SNR start value", "30", MODEMPAR_NUMERIC, { n: { 0, 100, 1, 10 } } },
	{ "snrend", "SNR End", "SNR end value", "5", MODEMPAR_NUMERIC, { n: { 0, 100, 1, 10 } } },
	{ "snrstep", "SNR Step", "SNR increment/decrement", "-2", MODEMPAR_NUMERIC, { n: { -100, 100, 1, 10 } } },
	{ "bits", "Simulation Bits", "Number of Bits for Simulation", "10000", MODEMPAR_NUMERIC, { n: { 10, 1000000000, 10000, 1000000 } } },
	{ NULL }
};

void pktinit(struct modemchannel *chan, const char *params[])
{
	if (params[0])
		simstate.snr = strtod(params[0], NULL);
	if (params[1])
		simstate.snrend = strtod(params[1], NULL);
	if (params[2])
		simstate.snrstep = strtod(params[2], NULL);
	if (params[3])
		simstate.nrbits = strtoul(params[3], NULL, 0);
}

void pktinitmkiss(struct modemchannel *chan, const char *params[])
{
	pktinit(chan, params);
}

#define DESCRAM10_TAPSH1  0
#define DESCRAM10_TAPSH2  7
#define DESCRAM10_TAPSH3  10

#define SCRAM10_TAP1  1
#define SCRAM10_TAPN  ((1<<DESCRAM10_TAPSH3)|(1<<(DESCRAM10_TAPSH3-DESCRAM10_TAPSH2)))

static inline unsigned int hweight8(unsigned char w)
{
        unsigned char res = (w & 0x55) + ((w >> 1) & 0x55);
        res = (res & 0x33) + ((res >> 2) & 0x33);
        return (res & 0x0F) + ((res >> 4) & 0x0F);
}

int pktget(struct modemchannel *chan, unsigned char *data, unsigned int len)
{
	unsigned int mask1, mask2, i, j;

	if (simstate.rxcnt >= simstate.nrbits ||
	    simstate.txcnt >= simstate.nrbits+TXDELAYBITS+TXTAILBITS)
		return 0;
	for (i = 0; i < len; i++, data++) {
		for (j = 0, mask1 = SCRAM10_TAP1, mask2 = SCRAM10_TAPN; j < 8; j++, mask1 <<= 1, mask2 <<= 1)
			if (simstate.txshreg & mask1)
				simstate.txshreg ^= mask2;
		data[0] = simstate.txshreg;
		simstate.txshreg >>= 8;
		simstate.txcnt += 8;
		if (!(simstate.txcnt & 1023)) {
			fprintf(stderr, "%7u %7u %7u\r", simstate.txcnt, simstate.rxcnt, simstate.rxerr);
			fflush(stderr);
		}
	}
	return simstate.terminate ? 0 : len;
}

void pktput(struct modemchannel *chan, const unsigned char *data, unsigned int len)
{
	unsigned int i, j;

	for (i = 0; i < len; i++, data++) {
		if (simstate.rxcnt >= simstate.nrbits)
			return;
		simstate.rxshreg |= ((unsigned int)data[0]) << DESCRAM10_TAPSH3;
		j = ((simstate.rxshreg >> DESCRAM10_TAPSH1) ^
		     (simstate.rxshreg >> DESCRAM10_TAPSH2) ^
		     (simstate.rxshreg >> DESCRAM10_TAPSH3)) & 0xff;
		simstate.rxshreg >>= 8;
		if (simstate.txcnt < TXDELAYBITS)
			continue;
		simstate.rxcnt += 8;
		if (j)
			simstate.rxerr += hweight8(j);
	}
}

void pkttransmitloop(struct state *state)
{
	struct modemchannel *chan;
	unsigned int nrtx;
	time_t tstart, tend;

#ifndef WIN32
	signal(SIGHUP, sigterm);
#endif
	pttsetptt(&state->ptt, 1);
	simstate.rxcnt = simstate.rxerr = simstate.txcnt = 0;
	iosetsnr(state, simstate.snr);
	logprintf(MLOG_INFO, "Simulation: SNR range %.3f...%.3f step %.3f  bits %u\n", 
		  simstate.snr, simstate.snrend, simstate.snrstep, simstate.nrbits);
	printf("%% Simulation: SNR range %.3f...%.3f step %.3f  bits %u\n"
	       "%% snr nrbits txcnt rxcnt rxerr\n"
	       "result = [\n",
	       simstate.snr, simstate.snrend, simstate.snrstep, simstate.nrbits);
	time(&tstart);
	while (!simstate.terminate) {
		if (simstate.rxcnt >= simstate.nrbits ||
		    simstate.txcnt >= simstate.nrbits+TXDELAYBITS+TXTAILBITS) {
			printf(" %10.5f %7u %7u %7u %7u\n",
			       simstate.snr, simstate.nrbits, simstate.txcnt, simstate.rxcnt, simstate.rxerr);
			fflush(stdout);
			simstate.snr += simstate.snrstep;
			simstate.rxcnt = simstate.rxerr = simstate.txcnt = 0;
			if (simstate.snrstep < 0) {
				if (simstate.snr < simstate.snrend)
					break;
			} else {
				if (simstate.snr > simstate.snrend)
					break;
			}
			iosetsnr(state, simstate.snr);
		}
		nrtx = 0;
		for (chan = state->channels; chan; chan = chan->next)
			if (chan->mod) {
				chan->mod->modulate(chan->modstate, 0);
				nrtx++;
			}
		if (!nrtx)
			break;
	}
	time(&tend);
	printf("]\n"
	       "%% simulation time: %lu seconds\n"
	       "semilogy(result(:,1),result(:,5)/3/result(:,4));\n", (long unsigned int)(tend-tstart));
	pttsetptt(&state->ptt, 0);
}

void pktsetdcd(struct modemchannel *chan, int dcd)
{
	pttsetdcd(&chan->state->ptt, dcd);
}

void pktrelease(struct modemchannel *chan)
{
}

void p3dreceive(struct modemchannel *chan, const unsigned char *pkt, u_int16_t crc)
{
}

void p3drxstate(struct modemchannel *chan, unsigned int synced, unsigned int carrierfreq)
{
	pktsetdcd(chan, !!synced);
}

/* ---------------------------------------------------------------------- */
