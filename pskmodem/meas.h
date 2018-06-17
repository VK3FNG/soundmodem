/*****************************************************************************/

/*
 *      meas.h  --  Measurement utility.
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

#ifndef _MEAS_H
#define _MEAS_H

/* ---------------------------------------------------------------------- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

/* ---------------------------------------------------------------------- */

/*
 * Bittypes
 */

#ifndef HAVE_BITTYPES

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
typedef int int8_t __attribute__((__mode__(__QI__)));
typedef unsigned int u_int8_t __attribute__((__mode__(__QI__)));
typedef int int16_t __attribute__((__mode__(__HI__)));
typedef unsigned int u_int16_t __attribute__((__mode__(__HI__)));
typedef int int32_t __attribute__((__mode__(__SI__)));
typedef unsigned int u_int32_t __attribute__((__mode__(__SI__)));
typedef int int64_t __attribute__((__mode__(__DI__)));
typedef unsigned int u_int64_t __attribute__((__mode__(__DI__)));
#else
typedef char /* deduced */ int8_t __attribute__((__mode__(__QI__)));
typedef unsigned char /* deduced */ u_int8_t __attribute__((__mode__(__QI__)));
typedef short /* deduced */ int16_t __attribute__((__mode__(__HI__)));
typedef unsigned short /* deduced */ u_int16_t __attribute__((__mode__(__HI__)));
typedef long /* deduced */ int32_t __attribute__((__mode__(__SI__)));
typedef unsigned long /* deduced */ u_int32_t __attribute__((__mode__(__SI__)));
typedef long long /* deduced */ int64_t __attribute__((__mode__(__DI__)));
typedef unsigned long long /* deduced */ u_int64_t __attribute__((__mode__(__DI__)));
#endif

#endif /* !HAVE_BITTYPES */

/* ---------------------------------------------------------------------- */
#endif /* _MEAS_H */
