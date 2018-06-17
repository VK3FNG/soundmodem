/*****************************************************************************/

/*
 *      pktsimple.c  --  Simple packet generator/sink.
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

static unsigned int terminate = 0;

static RETSIGTYPE sigterm()
{
	terminate = 1;
}

struct modemparams pktmkissparams[] = {
	{ NULL }
};

struct modemparams pktkissparams[] = {
	{ NULL }
};

void pktinit(struct modemchannel *chan, const char *params[])
{
	return;
}

void pktinitmkiss(struct modemchannel *chan, const char *params[])
{
	return;
}

int pktget(struct modemchannel *chan, unsigned char *data, unsigned int len)
{
	static unsigned int pktnr = 0;
	unsigned int i;

#if 0
	memset(data, 0x55, len);
	return terminate ? 0 : len;
#endif
	if (len < 5) {
		for (i = 0; i < len; i++)
			data[i] = '@' + i;
		return terminate ? 0 : len;
	}
	snprintf(data, len, "%04X", pktnr);
	pktnr = (pktnr + 1) & 0xffff;
	for (i = 4; i < len; i++)
		data[i] = '@' - 4 + i;
	return terminate ? 0 : len;
}

void pktput(struct modemchannel *chan, const unsigned char *data, unsigned int len)
{
	unsigned int i;

	fprintf(stderr, "Data: ");
	for (i = 0; i < len; i++)
#if 1
		if (data[i] >= ' ' && data[i] <= 0x7f)
			putc(data[i], stderr);
		else
			putc('.', stderr);
#else
	fprintf(stderr, "%02x", data[i]);
#endif
	fprintf(stderr, "\n");
}

void pkttransmitloop(struct state *state)
{
	struct modemchannel *chan;
	unsigned int nrtx;

#ifndef WIN32
	signal(SIGHUP, sigterm);
#endif
	pttsetptt(&state->ptt, 1);
	while (!terminate) {
		nrtx = 0;
		for (chan = state->channels; chan; chan = chan->next)
			if (chan->mod) {
				chan->mod->modulate(chan->modstate, 0);
				nrtx++;
			}
		if (!nrtx)
			break;
	}
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
