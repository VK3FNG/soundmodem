/*****************************************************************************/

/*
 *      testcrc.c  --  Test the AMSAT CRC table.
 *
 *      Copyright (C) 2000  Thomas Sailer (sailer@ife.ee.ethz.ch)
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

#include "p3dtbl.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* --------------------------------------------------------------------- */

#define BLOCKSZ  512

static int testone(void)
{
	unsigned char data[BLOCKSZ];
	unsigned int i, j;
	u_int16_t crc1 = 0xffff, crc2 = 0xffff;
	
	/* fill block with random bytes */
	for (i = 0; i < BLOCKSZ; i++)
		data[i] = random();
	/* compute conventional CRC */
	for (i = 0; i < BLOCKSZ; i++)
		for (j = 0; j < 8; j++) {
			crc1 = (crc1 << 1) | (((crc1 >> 15) ^ (data[i] >> (7-j))) & 1);
			if (crc1 & 1)
				crc1 ^= (1 << 5) | (1 << 12);
		}
	/* compute table CRC */
	for (i = 0; i < BLOCKSZ; i++)
		crc2 = (crc2 << 8) ^ amsat_crc[((crc2 >> 8) ^ data[i]) & 0xff];
	if (crc1 == crc2)
		return 0;
	printf("CRC error!  conventional CRC 0x%04x, table CRC 0x%04x\n", crc1, crc2);
	for (i = 0; i < BLOCKSZ; i++) {
		if (!(i & 15))
			printf("\n%04x:", i);
		printf(" %02x", data[i]);
	}
	printf("\n");
	return -1;
}

/* --------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	unsigned int i;

	srandom(time(NULL));
	for (i = 0; i < 131072; i++) {
		if (!(i & 1023)) {
			printf("%u\r", i);
			fflush(stdout);
		}
		if (testone())
			exit(1);
	}
	printf("OK\n");
	exit(0);
}
