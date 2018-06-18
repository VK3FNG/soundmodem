/*****************************************************************************/

/*
 *      simd.h  --  SIMD filter procedures (aka MMX, VIS, etc).
 *
 *      Copyright (C) 1999-2000  Thomas Sailer (sailer@ife.ee.ethz.ch)
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

#ifndef _SIMD_H
#define _SIMD_H

/* ---------------------------------------------------------------------- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "modem.h"

/* ---------------------------------------------------------------------- */

/*
 * SIMD stuff
 */

#if !defined(__i386__)
#undef USEMMX
#endif

#if !defined(__sparc__) && !defined(__sparc64__)
#undef USEVIS
#endif

#if !defined(USEMMX) && !defined(USEVIS)

static inline void initsimd(int enable)
{
}

static inline int checksimd(void)
{
	return 0;
}

static inline int simdfir16(const int16_t *p1, const int16_t *p2, unsigned int nr)
{
	int s = 0;

	for (; nr > 0; nr--, p1++, p2++)
		s += (*p1) * (*p2);
	return s;
}

static inline void simdpreparefpu(void)
{
}

#else

extern unsigned int simd_enabled;

extern void initsimd(int enable);

static inline int checksimd(void)
{
	return simd_enabled;
}

#if defined(USEMMX)

#define MMXCLOBBER "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)"

static inline int simdfir16(const int16_t *p1, const int16_t *p2, unsigned int nr)
{
	unsigned int i, j;
	int s = 0;

	if (checksimd()) {
		j = nr >> 2;
		nr &= 3;
		asm volatile("pxor\t%%mm0,%%mm0" : : : MMXCLOBBER);
		for (i = 0; i < j; i++, p1 += 4, p2 += 4)
			asm volatile("\n\tmovq\t%0,%%mm1"
				     "\n\tpmaddwd\t%1,%%mm1"
				     "\n\tpaddd\t%%mm1,%%mm0" : : "m" (*p1), "m" (*p2) : MMXCLOBBER);
		asm volatile("\n\tmovq\t%%mm0,%%mm1 "
			     "\n\tpunpckhdq\t%%mm1,%%mm1"
			     "\n\tpaddd\t%%mm1,%%mm0"
			     "\n\tmovd\t%%mm0,%0" : "=m" (s) : : MMXCLOBBER);
	}
	for (; nr > 0; nr--, p1++, p2++)
		s += (*p1) * (*p2);
	return s;
}

static inline void simdpreparefpu(void)
{
	if (checksimd())
		asm volatile("emms");
}

#elif defined(USEVIS)

/*extern inline*/static int simdfir16(const int16_t *p1, const int16_t *p2, unsigned int nr)
{
	double dsum1, dsum2, dsum3, dsum4, arg1, arg2, arg3, arg4;
	float sum, sum1, sum2;
	unsigned int i, j;
	int s = 0, sx[1];

	if (checksimd()) {
		j = nr >> 1;
		nr &= 1;
		asm("fzeros %0" : "=f" (sum));
		for (i = 0; i < j; i++) {
			asm volatile("ldda [%2] 0xd2, %0 ! ASI_FL16_P \n\t"
				     "ldda [%3] 0xd2, %1 ! ASI_FL16_P \n\t": "=e" (arg1), "=e" (arg2) : "r" (p1), "r" (p2));
			p1++; p2++;
			asm volatile("ldda [%2] 0xd2, %0 ! ASI_FL16_P \n\t"
				     "ldda [%3] 0xd2, %1 ! ASI_FL16_P \n\t": "=e" (arg3), "=e" (arg4) : "r" (p1), "r" (p2));
			p1++; p2++;
                        asm volatile("fmuld8sux16 %L2,%L3,%0 \n\t"
				     "fmuld8ulx16 %L2,%L3,%1 \n\t" : "=&e" (dsum1), "=&e" (dsum2) : "e" (arg1), "e" (arg2));
			asm volatile("fmuld8sux16 %L2,%L3,%0 \n\t"
				     "fmuld8ulx16 %L2,%L3,%1 \n\t" : "=&e" (dsum3), "=&e" (dsum4) : "e" (arg3), "e" (arg4));
			asm volatile("fpadd32s %L1,%L2,%0" : "=f" (sum1) : "e" (dsum1), "e" (dsum2));
			asm volatile("fpadd32s %L1,%L2,%0" : "=f" (sum2) : "e" (dsum3), "e" (dsum4));
			asm volatile("fpadd32s %1,%2,%0" : "=f" (sum) : "f" (sum1), "f" (sum));
			asm volatile("fpadd32s %1,%2,%0" : "=f" (sum) : "f" (sum2), "f" (sum));
		};
		asm("st %1,%0" : "=m" (sx) : "f" (sum));
		s = sx[0];
	}
	for (; nr > 0; nr--, p1++, p2++)
		s += (*p1) * (*p2);
	return s;
}

static inline void simdpreparefpu(void)
{
}

#endif

#endif

/* ---------------------------------------------------------------------- */
#endif /* _SIMD_H */
