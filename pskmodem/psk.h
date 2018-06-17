/*****************************************************************************/

/*
 *      psk.h  --  Defines for the PSK modem.
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

#ifndef _PSK_H
#define _PSK_H

/* ---------------------------------------------------------------------- */

#define SAMPLERATE    9600

/* ---------------------------------------------------------------------- */

typedef struct {
        short re;
        short im;
} cplxshort_t;

typedef struct {
        int re;
        int im;
} cplxint_t;

/* ---------------------------------------------------------------------- */

#define FCARRIER      1800
#define SYMRATE       2400
#define SYMBITS          3
#define TRAINSYMS       16
#define OBSTRAINSYMS ((TRAINSYMS)-(CHANNELLEN)+1)
#define DATABYTES       48
#define DATABITS     ((DATABYTES) * 8)
#define DATASYMS     (((DATABITS)+(SYMBITS)-1)/(SYMBITS))
#define CHANNELLEN       3

#define SYMBITMASK     ((1<<(SYMBITS))-1)


/* TxFilter */
#define TXFILTLEN       16
#define TXFILTOVERBITS  4
#define TXFILTOVER      (1<<(TXFILTOVERBITS))
#define TXFILTFIDX(x)   (((x)>>(16-(TXFILTOVERBITS)))&(TXFILTOVER-1))
#define TXFILTFSAMP(x)  ((x)>>16)

/* RxFilter */
#define RXFILTLEN       16
#define RXFILTOVERBITS   3
#define RXFILTOVER      (1<<(RXFILTOVERBITS))
#define RXFILTFIDX(x)   (((x)>>(16-(RXFILTOVERBITS)))&(RXFILTOVER-1))
#define RXFILTFSAMP(x)  ((x)>>16)

/* ---------------------------------------------------------------------- */

extern const cplxshort_t psk_symmapping[1<<(SYMBITS)];

/* ---------------------------------------------------------------------- */

#define MLSENRSYMB     ((CHANNELLEN)-1)
#define MLSENODES      (1<<((MLSENRSYMB)*(SYMBITS)))
#define MLSEMTABSIZE   (1<<(((MLSENRSYMB)+1)*(SYMBITS)))
#define MLSESRCINC     (1<<(((MLSENRSYMB)-1)*(SYMBITS)))
#define MLSEENERGYSH   6

typedef union {
	struct metrictab {
		int re;
		int im;
	} m[MLSEMTABSIZE];
	unsigned int simdm[MLSEMTABSIZE];
} metrictab_t;

extern void pskmlse_initmetric(const cplxshort_t *channel, metrictab_t *metrictab);
extern void pskmlse_trellis(unsigned int *nodemetric1, unsigned int *nodemetric2, metrictab_t *metrictab, unsigned short *backptr, int vr, int vi);

/* ---------------------------------------------------------------------- */
#endif /* _PSK_H */

