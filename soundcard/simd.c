/*****************************************************************************/

/*
 *      simd.c  --  SIMD (aka MMX and VIS) routines.
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

#define _GNU_SOURCE
#define _REENTRANT

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
#pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif

#include "modem.h"

/* ---------------------------------------------------------------------- */

#if defined(USEMMX)

unsigned int simd_enabled = 0;

static inline unsigned int cpucapability(void)
{
	unsigned int a, b, c, d;

	asm("pushfl \n\t"
	    "popl %0 \n\t"
	    "movl %0,%1 \n\t"
	    "xorl $0x00200000,%0 \n\t"
	    "pushl %0 \n\t"
	    "popfl \n\t"
	    "pushfl \n\t"
	    "popl %0 \n\t"
	    : "=r" (a), "=r" (b));
	if (a == b)
		return 0;
	asm("cpuid" : "=a" (a), "=b" (b), "=c" (c), "=d" (d) : "0" (1));
	return d;
}

void initsimd(int enable)
{
	unsigned int cap = cpucapability();

	simd_enabled = (enable && (cap & 0x800000)) ? 1 : 0;
	logprintf(3, "x86 CPU capability %08x, MMX %s%sabled\n", cap, enable ? "" : "manually ",
		  simd_enabled ? "en" : "dis");
}

#elif defined(USEVIS)

unsigned int simd_enabled = 0;

void initsimd(int enable)
{
	simd_enabled = !!enable;
	logprintf(3, "VIS %sabled\n", simd_enabled ? "en" : "dis");
}

#endif
