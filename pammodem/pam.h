/*****************************************************************************/

/*
 *      pam.h  --  Defines for the PAM modem.
 *
 *      Copyright (C) 1999  Thomas Sailer (sailer@ife.ee.ethz.ch)
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

#ifndef _PAM_H
#define _PAM_H

/* ---------------------------------------------------------------------- */

#include "modem.h"

/* ---------------------------------------------------------------------- */

#define SAMPLERATE   22050
//#define SAMPLERATE   19200
#define BITRATE       9600
#define TRAINBITS       32
#define OBSTRAINBITS (TRAINBITS-CHANNELLEN+1)
#define DATABYTES       64
#define DATABITS     (DATABYTES * 8)
#define CHANNELLEN       8

/* TxFilter */
#define TXFILTLEN       16
#define TXFILTOVERBITS  4
#define TXFILTOVER      (1<<(TXFILTOVERBITS))
#define TXFILTFIDX(x)   (((x)>>(16-(TXFILTOVERBITS)))&(TXFILTOVER-1))
#define TXFILTFSAMP(x)  ((x)>>16)

/* RxFilter */
#define RXFILTLEN       32
#define RXFILTOVERBITS   3
#define RXFILTOVER      (1<<(RXFILTOVERBITS))
#define RXFILTFIDX(x)   (((x)>>(16-(RXFILTOVERBITS)))&(RXFILTOVER-1))
#define RXFILTFSAMP(x)  ((x)>>16)

/* ---------------------------------------------------------------------- */
#endif /* _PAM_H */
