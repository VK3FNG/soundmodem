/*****************************************************************************/

/*
 *      p3d.h  --  Defines for the AO-40 P3D PSK modem.
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

#ifndef _P3D_H
#define _P3D_H

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

#define FNUMBER         15
#define FCENTER       1600
#define FSPACING        50
#define SYMRATE        400

#define SYNCWORD     0x3915ED30

/* RxFilter */
#define RXFILTSPAN        4
#define RXFILTLEN       256
#define RXFILTOVERBITS    3
#define RXFILTOVER      (1<<(RXFILTOVERBITS))
#define RXFILTFIDX(x)   (((x)>>(16-(RXFILTOVERBITS)))&(RXFILTOVER-1))
#define RXFILTFSAMP(x)  ((x)>>16)

/* ---------------------------------------------------------------------- */
#endif /* _P3D_H */
