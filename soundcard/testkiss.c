/*****************************************************************************/

/*
 *      testkiss.c  --  Test the HDLC/KISS encoder/decoder.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "soundio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

/* ---------------------------------------------------------------------- */

static struct modemchannel chan = {
	NULL, &state, NULL, NULL, NULL, NULL, 0, 
#if 0
	/* gcc3 no longer accepts {} as initializer */
	{}, { 0, }
#endif
};

struct state state = {
	&chan, NULL, NULL,
	{ 150, 40, 100, 0 }, {}
};

/* ---------------------------------------------------------------------- */

#define DATABYTES 64

void iotransmitstart(struct audioio *audioio)
{
}

void iotransmitstop(struct audioio *audioio)
{
}

void pttsetptt(struct pttio *state, int pttx)
{
}

void pttsetdcd(struct pttio *state, int dcd)
{
}

static void loopback(void)
{
	unsigned char tmp[DATABYTES];
	unsigned char *bp;
	unsigned int i, j, k;

	pktsetdcd(state.channels, 0);
	for (;;) {
		if (pktget(state.channels, tmp, sizeof(tmp))) {
			for (bp = tmp, i = 0; i < sizeof(tmp); i++, bp++) {
				j = *bp;
				for (k = 0; k < 8; k++, j >>= 1)
					putc_unlocked('0' + (j & 1), stdout);
			}
			printf("\n");
			pktput(state.channels, tmp, sizeof(tmp));
		}
	}
}

/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	unsigned int verblevel = ~0, tosyslog = 0;
	const char *par[16] = { NULL, };
	int c, err = 0, mkiss = 0;

        while ((c = getopt(argc, argv, "v:sd:m")) != EOF) {
                switch (c) {
                case 'v':
                        verblevel = strtoul(optarg, NULL, 0);
                        break;

		case 's':
			tosyslog = 1;
			break;

		case 'd':
			par[0] = optarg;
			break;

#ifdef HAVE_MKISS
		case 'm':
			mkiss = 1;
			break;
#endif

                default:
                        err++;
                        break;
                }
        }
        if (err) {
                fprintf(stderr, "usage: [-v <verblevel>] [-s] [-K <kissdevice>]\n");
                exit(1);
        }
	loginit(verblevel, tosyslog);
#ifdef HAVE_MKISS
	if (mkiss) {
		pktinitmkiss(state.channels, par);
	} else
#endif
		pktinit(state.channels, par);
	loopback();
	exit(0);
}
