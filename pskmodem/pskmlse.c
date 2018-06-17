/*****************************************************************************/

/*
 *      pskmlse.c  --  PSK modem Maximum Likelyhood Sequence Estimator.
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

#include "modem.h"
#include "psk.h"
#include "simd.h"

#include <stdlib.h>

/* ---------------------------------------------------------------------- */

/*
 * Maximum Likelyhood Sequence Estimation
 */

/* ---------------------------------------------------------------------- */

/* MMX MLSE */

#if defined(USEMMX)

#define VEMMXINIT(vx,vy)                                                                          \
do {                                                                                              \
       asm volatile("movd %0,%%mm0 \n\t"                                                          \
		    "punpckldq %%mm0,%%mm0 \n\t"                                                  \
		    "movd %1,%%mm1 \n\t"                                                          \
		    "punpckldq %%mm1,%%mm1 \n\t"                                                  \
		    : : "r" ((((int)vx) & 0xffff) | (((int)vy) << 16)), "r" (MLSESRCINC));        \
} while (0)

#define VEMMXINTER0(metrictab,nodemetric1,x)                                                      \
do {                                                                                              \
       asm volatile("movq %0,%%mm4 \n\t"       /* load metrictab values */                        \
		    "movd %3,%%mm2 \n\t"       /* first iteration: set index register */          \
		    "punpckldq %%mm2,%%mm2 \n\t"                                                  \
		    "movq %%mm2,%%mm3 \n\t"    /* first iteration: set counting index register */ \
		    "psubw %%mm0,%%mm4 \n\t"   /* subtract current data value */                  \
		    "pmaddwd %%mm4,%%mm4 \n\t" /* square and add -> energy */                     \
		    "psrld %2,%%mm4 \n\t"      /* shift by energy divisor */                      \
                    "movd %1,%%mm6 \n\t"       /* load nodemetric1 */                             \
                    "punpckldq %%mm6, %%mm6 \n\t"  /* duplicate it */                             \
		    "paddd %%mm6,%%mm4 \n\t"   /* add nodemetric1 */                              \
		    : : "m" (metrictab[0]), "m" (nodemetric1[0]), "i" (MLSEENERGYSH),             \
		    "r" (x));                                                                     \
} while (0)

#define VEMMXINTERN(metrictab,nodemetric1,n)                                                      \
do {                                                                                              \
       asm volatile("movq %0,%%mm7 \n\t"       /* load metrictab values */                        \
		    "paddw %%mm1,%%mm3 \n\t"   /* increment counting index register */            \
		    "psubw %%mm0,%%mm7 \n\t"   /* subtract current data value */                  \
		    "pmaddwd %%mm7,%%mm7 \n\t" /* square and add -> energy */                     \
		    "psrld %2,%%mm7 \n\t"      /* shift by energy divisor */                      \
                    "movd %1,%%mm6 \n\t"       /* load nodemetric1 */                             \
                    "punpckldq %%mm6, %%mm6 \n\t"  /* duplicate it */                             \
		    "paddd %%mm6,%%mm7 \n\t"   /* add nodemetric1 */                              \
		    "movq %%mm7,%%mm6 \n\t"                                                       \
		    "pcmpgtd %%mm4,%%mm7 \n\t" /* compare new metrics with old ones */            \
		    "movq %%mm7,%%mm5 \n\t"    /* 0xffff if new ones are greater */               \
		    "pand %%mm7,%%mm4 \n\t"    /* mux for metric values and index values */       \
		    "pandn %%mm6,%%mm5 \n\t"                                                      \
		    "por %%mm5,%%mm4 \n\t"                                                        \
		    "pand %%mm7,%%mm2 \n\t"                                                       \
		    "pandn %%mm3,%%mm7 \n\t"                                                      \
		    "por %%mm7,%%mm2 \n\t"                                                        \
		    : : "m" (metrictab[(n)*2]), "m" (nodemetric1[(n)*MLSESRCINC]),                \
		    "i" (MLSEENERGYSH));                                                          \
} while (0)

#define VEMMXINTEREND(nodemetric2,backptr)                                                        \
do {                                                                                              \
       asm volatile("movq %%mm4,%0 \n\t"                                                          \
		    "movq %%mm2,%%mm7 \n\t"                                                       \
		    "psrlq $32,%%mm7\n\t"                                                         \
		    "punpcklwd %%mm7,%%mm2 \n\t"                                                  \
		    "movd %%mm2,%1 \n\t"                                                          \
		    : : "m" (nodemetric2[0]), "m" (backptr[0]) : "memory");                       \
} while (0)

#if SYMBITS == 1

static void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
	unsigned int x, i;

	VEMMXINIT(vr,vi);
	for (x = 0; x < MLSESRCINC; x++) {
		for (i = 0; i < ((1<<SYMBITS)>>1); i++) {
			VEMMXINTER0(metrictab,nodemetric1,x);
			VEMMXINTERN(metrictab,nodemetric1,1);
			VEMMXINTEREND(nodemetric2,backptr);
			nodemetric2 += 2;
			backptr += 2;
			metrictab += 2*(1<<SYMBITS);
		}
		nodemetric1++;
	}
	asm volatile("emms");
}

#elif SYMBITS == 2

static void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
	unsigned int x, i;

	VEMMXINIT(vr,vi);
	for (x = 0; x < MLSESRCINC; x++) {
		for (i = 0; i < ((1<<SYMBITS)>>1); i++) {
			VEMMXINTER0(metrictab,nodemetric1,x);
			VEMMXINTERN(metrictab,nodemetric1,1);
			VEMMXINTERN(metrictab,nodemetric1,2);
			VEMMXINTERN(metrictab,nodemetric1,3);
			VEMMXINTEREND(nodemetric2,backptr);
			nodemetric2 += 2;
			backptr += 2;
			metrictab += 2*(1<<SYMBITS);
		}
		nodemetric1++;
	}
	asm volatile("emms");
}

#elif SYMBITS == 3

#define DBGPRNT(mt)                                                                            \
do {                                                                                           \
	unsigned long long m, mm2, mm3, mm4, mm7;                                              \
	asm("movq %%mm2,%0\n\tmovq %%mm3,%1\n\tmovq %%mm4,%2\n\t"                              \
	    "movq %4,%%mm7\n\tpsubw %%mm0,%%mm7\n\tpmaddwd %%mm7,%%mm7\n\tmovq %%mm7,%3"       \
	    : "=m" (mm2), "=m" (mm3), "=m" (mm4), "=m" (mm7) : "m" ((mt)[0]));                 \
	m = *(unsigned long long *)(mt);                                                       \
	printf("x %02x i %02x mt %016llx mm2 %016llx mm3 %016llx mm4 %016llx mm7 %016llx\n",   \
	       x, i, m, mm2, mm3, mm4, mm7);                                                   \
} while (0)

#define DBGPRNT2(vr,vi)                                                                       \
do {                                                                                          \
	unsigned long long mm0, mm1;                                                          \
	asm("movq %%mm0,%0\n\tmovq %%mm1,%1" : "=m" (mm0), "=m" (mm1));                       \
	printf("mmxtrellis start vr %4d vi %4d mm0 %016llx mm1 %016llx\n", vr, vi, mm0, mm1); \
} while (0)

#undef DBGPRNT
#undef DBGPRNT2
#define DBGPRNT(mt)      do { } while (0)
#define DBGPRNT2(vr,vi)  do { } while (0)

static void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
	unsigned int x, i;

	VEMMXINIT(vr,vi);
	DBGPRNT2(vr,vi);
	for (x = 0; x < MLSESRCINC; x++) {
		for (i = 0; i < ((1<<SYMBITS)>>1); i++) {
			VEMMXINTER0(metrictab,nodemetric1,x);
			DBGPRNT(metrictab);
			VEMMXINTERN(metrictab,nodemetric1,1);
			DBGPRNT(metrictab+2);
			VEMMXINTERN(metrictab,nodemetric1,2);
			DBGPRNT(metrictab+4);
			VEMMXINTERN(metrictab,nodemetric1,3);
			DBGPRNT(metrictab+6);
			VEMMXINTERN(metrictab,nodemetric1,4);
			DBGPRNT(metrictab+8);
			VEMMXINTERN(metrictab,nodemetric1,5);
			DBGPRNT(metrictab+10);
			VEMMXINTERN(metrictab,nodemetric1,6);
			DBGPRNT(metrictab+12);
			VEMMXINTERN(metrictab,nodemetric1,7);
			DBGPRNT(metrictab+14);
			VEMMXINTEREND(nodemetric2,backptr);
			nodemetric2 += 2;
			backptr += 2;
			metrictab += 2*(1<<SYMBITS);
		}
		nodemetric1++;
	}
	asm volatile("emms");
}

#elif SYMBITS == 4

static void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
	unsigned int x, i;

	VEMMXINIT(vr,vi);
	for (x = 0; x < MLSESRCINC; x++) {
		for (i = 0; i < ((1<<SYMBITS)>>1); i++) {
			VEMMXINTER0(metrictab,nodemetric1,x);
			VEMMXINTERN(metrictab,nodemetric1,1);
			VEMMXINTERN(metrictab,nodemetric1,2);
			VEMMXINTERN(metrictab,nodemetric1,3);
			VEMMXINTERN(metrictab,nodemetric1,4);
			VEMMXINTERN(metrictab,nodemetric1,5);
			VEMMXINTERN(metrictab,nodemetric1,6);
			VEMMXINTERN(metrictab,nodemetric1,7);
			VEMMXINTERN(metrictab,nodemetric1,8);
			VEMMXINTERN(metrictab,nodemetric1,9);
			VEMMXINTERN(metrictab,nodemetric1,10);
			VEMMXINTERN(metrictab,nodemetric1,11);
			VEMMXINTERN(metrictab,nodemetric1,12);
			VEMMXINTERN(metrictab,nodemetric1,13);
			VEMMXINTERN(metrictab,nodemetric1,14);
			VEMMXINTERN(metrictab,nodemetric1,15);
			VEMMXINTEREND(nodemetric2,backptr);
			nodemetric2 += 2;
			backptr += 2;
			metrictab += 2*(1<<SYMBITS);
		}
		nodemetric1++;
	}
	asm volatile("emms");
}

#endif

/* ---------------------------------------------------------------------- */

/* VIS MLSE */

#elif defined(USEVIS)

#define VEVISSTATE                                                                                         \
       double value, admask, nmetric, btrk, btinc, btcount;

#define VEVISINIT(vx,vy)                                                                                   \
do {                                                                                                       \
       int v = (((int)vx) & 0xffff) | (((int)vy) << 16);                                                   \
       unsigned int inc = MLSESRCINC | (MLSESRCINC << 16);                                                 \
       unsigned int adm = 0x00ffffff;                                                                      \
       asm volatile("ld %2,%L0 \n\t"                                                                       \
		    "ld %2,%H0 \n\t"                                                                       \
		    "ld %3,%L1 \n\t"                                                                       \
		    "wr %%g0,%4,%%gsr \n\t"                                                                \
		    : "=e" (value), "=e" (btinc) : "m" (v), "m" (inc), "g" (7));                           \
       asm volatile("ld %1,%L0 \n\t"                                                                       \
		    "ld %1,%H0 \n\t" : "=e" (admask) : "m" (adm));                                         \
} while (0)

#define VEVISINTER0(metrictab,nodemetric1,x)                                                               \
do {                                                                                                       \
       double mval0, mval8, mval1, mval2, mval3, mval4, mval5, mval6, nmetr;                               \
       int initbtcnt = (x) | ((x)<<16);                                                                    \
       asm volatile("ld %1,%L0 \n\t" : "=e" (btcount) : "m" (initbtcnt));                                  \
       asm volatile("ld %1,%L0 \n\t"              /* load metrictab value */                               \
		    "ld %2,%H0 \n\t"              /* subtract current data point */                        \
		    : "=e" (mval8) : "m" (metrictab[0]), "m" (metrictab[1]));                              \
       asm volatile("fpsub16 %2,%1,%0 \n\t"                                                                \
		    : "=e" (mval0) : "e" (value), "e" (mval8));                                            \
       asm volatile("ld %1,%L0 \n\t"               /* load node metric */                                  \
		    "fsrc1s %L0,%H0 \n\t"                                                                  \
		    : "=e" (nmetr) : "m" (nodemetric1[0]));                                                \
       asm volatile("fmuld8sux16 %L4,%L4,%0 \n\t"  /* squarer multiplications */                           \
		    "fmuld8ulx16 %L4,%L4,%1 \n\t"                                                          \
		    "fmuld8sux16 %H4,%H4,%2 \n\t"                                                          \
		    "fmuld8ulx16 %H4,%H4,%3 \n\t"                                                          \
		    "fpadd32 %0,%1,%0 \n\t"                                                                \
		    "fpadd32 %2,%3,%2 \n\t"                                                                \
		    "fpadd32s %L0,%H0,%L0 \n\t"                                                            \
		    "fpadd32s %L2,%H2,%H0 \n\t"                                                            \
		    : "=&e" (mval1), "=&e" (mval2), "=&e" (mval3), "=&e" (mval4) : "e" (mval0));           \
       asm volatile("faligndata %1,%1,%0 \n\t"                                                             \
		    "fand %0,%2,%0 \n\t"                                                                   \
		    : "=&e" (mval5) : "e" (mval1), "e" (admask));                                          \
       asm volatile("fpadd32 %2,%1,%0 \n\t" : "=e" (mval6) : "e" (mval5), "e" (nmetr));                    \
       asm volatile("std %2,%0 \n\t"                                                                       \
		    "std %3,%1 \n\t"                                                                       \
		    : "=m" (nmetric), "=m" (btrk) : "e" (mval6), "e" (btcount));                           \
} while (0)

#define VEVISINTERN(metrictab,nodemetric1,n)                                                               \
do {                                                                                                       \
       double mval0, mval8, mval1, mval2, mval3, mval4, mval5, mval6, mval7, nmetr;                        \
       unsigned int stmask;                                                                                \
       asm volatile("fpadd16s %L2,%L1,%L0 \n\t" : "=e" (btcount) : "e" (btinc), "e" (btcount));            \
       asm volatile("ld %1,%L0 \n\t"              /* load metrictab value */                               \
		    "ld %2,%H0 \n\t"              /* subtract current data point */                        \
		    : "=e" (mval8) : "m" (metrictab[2*(n)]), "m" (metrictab[2*(n)+1]));                    \
       asm volatile("fpsub16 %2,%1,%0 \n\t"                                                                \
		    : "=e" (mval0) : "e" (value), "e" (mval8));                                            \
       asm volatile("ld %1,%L0 \n\t"               /* load node metric */                                  \
		    "fsrc1s %L0,%H0 \n\t"                                                                  \
		    : "=e" (nmetr)                                                                         \
		    : "m" (nodemetric1[(n)*MLSESRCINC]));                                                  \
       asm volatile("fmuld8sux16 %L4,%L4,%0 \n\t"  /* squarer multiplications */                           \
		    "fmuld8ulx16 %L4,%L4,%1 \n\t"                                                          \
		    "fmuld8sux16 %H4,%H4,%2 \n\t"                                                          \
		    "fmuld8ulx16 %H4,%H4,%3 \n\t"                                                          \
		    "fpadd32 %0,%1,%0 \n\t"                                                                \
		    "fpadd32 %2,%3,%2 \n\t"                                                                \
		    "fpadd32s %L0,%H0,%L0 \n\t"                                                            \
		    "fpadd32s %L2,%H2,%H0 \n\t"                                                            \
		    : "=&e" (mval1), "=&e" (mval2), "=&e" (mval3), "=&e" (mval4) : "e" (mval0));           \
       asm volatile("faligndata %1,%1,%0 \n\t"                                                             \
		    "fand %0,%2,%0 \n\t"                                                                   \
		    : "=&e" (mval5) : "e" (mval1), "e" (admask));                                          \
       asm volatile("fpadd32 %2,%1,%0 \n\t" : "=e" (mval6) : "e" (mval5), "e" (nmetr));                    \
       asm volatile("ldd %3,%1 \n\t"                                                                       \
		    "fcmpgt32 %1,%2,%0 \n\t" : "=r" (stmask), "=&e" (mval7)                                \
		    : "e" (mval6), "m" (nmetric));                                                         \
       asm volatile("stda %2,[%0] %4, 0xc4 ! ASI_PST32_P \n\t"                                             \
		    "stda %3,[%1] %4, 0xc2 ! ASI_PST16_P \n\t"                                             \
		    : : "r" (&nmetric), "r" (&btrk), "e" (mval6), "e" (btcount), "r" (stmask) : "memory"); \
} while (0)

#define VEVISINTEREND(nodemetric2,backptr)                                                        \
do {                                                                                              \
       double tmp0, tmp1;                                                                         \
       unsigned	int tmpb[1];									  \
       asm("ldd %5,%0 \n\t"                                                                       \
	   "ldd %6,%1 \n\t"                                                                       \
	   "st %L0,%2 \n\t"                                                                       \
	   "st %H0,%3 \n\t"                                                                       \
	   "st %L1,%4 \n\t"                                                                       \
	   : "=&e" (tmp0), "=&e" (tmp1), "=m" (nodemetric2[0]), "=m" (nodemetric2[1]), "=m" (tmpb)  \
	   : "m" (nmetric), "m" (btrk) : "memory");                                               \
       backptr[0] = tmpb[0] >> 16;                                                                \
       backptr[1] = tmpb[0];                                                                      \
} while (0)

#if SYMBITS == 1

static void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
	unsigned int x, i;
	VEVISSTATE;

	VEVISINIT(vr,vi);
	for (x = 0; x < MLSESRCINC; x++) {
		for (i = 0; i < ((1<<SYMBITS)>>1); i++) {
			VEVISINTER0(metrictab,nodemetric1,x);
			VEVISINTERN(metrictab,nodemetric1,1);
			VEVISINTEREND(nodemetric2,backptr);
			nodemetric2 += 2;
			backptr += 2;
			metrictab += 2*(1<<SYMBITS);
		}
		nodemetric1++;
	}
}

#elif SYMBITS == 2

static void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
	unsigned int x, i;
	VEVISSTATE;

	VEVISINIT(vr,vi);
	for (x = 0; x < MLSESRCINC; x++) {
		for (i = 0; i < ((1<<SYMBITS)>>1); i++) {
			VEVISINTER0(metrictab,nodemetric1,x);
			VEVISINTERN(metrictab,nodemetric1,1);
			VEVISINTERN(metrictab,nodemetric1,2);
			VEVISINTERN(metrictab,nodemetric1,3);
			VEVISINTEREND(nodemetric2,backptr);
			nodemetric2 += 2;
			backptr += 2;
			metrictab += 2*(1<<SYMBITS);
		}
		nodemetric1++;
	}
}

#elif SYMBITS == 3

#define DBGPRNT(mt)                                                                            \
do {                                                                                           \
	unsigned long long m = *(unsigned long long *)(mt);                                    \
	printf("x %02x i %02x mt %016llx btcount %016llx nmetric %016llx btrk %016llx\n",      \
	       x, i, m, btcount, nmetric, btrk);                                               \
} while (0)

#define DBGPRNT2(vr,vi)                                                                       \
do {                                                                                          \
	printf("vistrellis start vr %4d vi %4d value %016llx admask %016llx btinc %016llx\n", \
	       vr, vi, value, admask, btinc);                                                 \
} while (0)

#undef DBGPRNT
#undef DBGPRNT2
#define DBGPRNT(mt)      do { } while (0)
#define DBGPRNT2(vr,vi)  do { } while (0)

static void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
	unsigned int x, i;
	VEVISSTATE;

	VEVISINIT(vr,vi);
	DBGPRNT2(vr,vi);
	for (x = 0; x < MLSESRCINC; x++) {
		for (i = 0; i < ((1<<SYMBITS)>>1); i++) {
			VEVISINTER0(metrictab,nodemetric1,x);
			DBGPRNT(metrictab);
			VEVISINTERN(metrictab,nodemetric1,1);
			DBGPRNT(metrictab+2);
			VEVISINTERN(metrictab,nodemetric1,2);
			DBGPRNT(metrictab+4);
			VEVISINTERN(metrictab,nodemetric1,3);
			DBGPRNT(metrictab+6);
			VEVISINTERN(metrictab,nodemetric1,4);
			DBGPRNT(metrictab+8);
			VEVISINTERN(metrictab,nodemetric1,5);
			DBGPRNT(metrictab+10);
			VEVISINTERN(metrictab,nodemetric1,6);
			DBGPRNT(metrictab+12);
			VEVISINTERN(metrictab,nodemetric1,7);
			DBGPRNT(metrictab+14);
			VEVISINTEREND(nodemetric2,backptr);
			nodemetric2 += 2;
			backptr += 2;
			metrictab += 2*(1<<SYMBITS);
		}
		nodemetric1++;
	}
}

#elif SYMBITS == 4

static void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
	unsigned int x, i;
	VEVISSTATE;

	VEVISINIT(vr,vi);
	for (x = 0; x < MLSESRCINC; x++) {
		for (i = 0; i < ((1<<SYMBITS)>>1); i++) {
			VEVISINTER0(metrictab,nodemetric1,x);
			VEVISINTERN(metrictab,nodemetric1,1);
			VEVISINTERN(metrictab,nodemetric1,2);
			VEVISINTERN(metrictab,nodemetric1,3);
			VEVISINTERN(metrictab,nodemetric1,4);
			VEVISINTERN(metrictab,nodemetric1,5);
			VEVISINTERN(metrictab,nodemetric1,6);
			VEVISINTERN(metrictab,nodemetric1,7);
			VEVISINTERN(metrictab,nodemetric1,8);
			VEVISINTERN(metrictab,nodemetric1,9);
			VEVISINTERN(metrictab,nodemetric1,10);
			VEVISINTERN(metrictab,nodemetric1,11);
			VEVISINTERN(metrictab,nodemetric1,12);
			VEVISINTERN(metrictab,nodemetric1,13);
			VEVISINTERN(metrictab,nodemetric1,14);
			VEVISINTERN(metrictab,nodemetric1,15);
			VEVISINTEREND(nodemetric2,backptr);
			nodemetric2 += 2;
			backptr += 2;
			metrictab += 2*(1<<SYMBITS);
		}
		nodemetric1++;
	}
}

#endif

/* ---------------------------------------------------------------------- */

#else

extern inline void simdtrellis(unsigned int *nodemetric1, unsigned int *nodemetric2, unsigned int *metrictab, unsigned short *backptr, int vr, int vi)
{
}

#endif

/* ---------------------------------------------------------------------- */

static inline unsigned int metric(struct metrictab *mtab, int vr, int vi)
{
        int x, y;
        x = vr - mtab->re;
        y = vi - mtab->im;
        return ((unsigned int)(x * x + y * y)) >> MLSEENERGYSH;
}

void pskmlse_trellis(unsigned int *nodemetric1, unsigned int *nodemetric2, metrictab_t *metrictab, unsigned short *backptr, int vr, int vi)
{
	struct metrictab *metrtab = metrictab->m;
        unsigned int *nmetr1;
        unsigned int xx, yy, xmm = 0, metricm, met, i;

	if (checksimd()) {
		simdtrellis(nodemetric1, nodemetric2, metrictab->simdm, backptr, vr, vi);
		return;
	}
        for (xx = 0; xx < MLSESRCINC; xx++) {
                for (yy = 0; yy < (1<<(SYMBITS)); yy++) {
                        nmetr1 = nodemetric1+xx;
                        metricm = ~0;
                        for (i = 0; i < MLSENODES; i += MLSESRCINC, nmetr1 += MLSESRCINC) {
                                met = nmetr1[0] + metric(metrtab, vr, vi);
                                metrtab++;
#if 0
                                printf("0x%03x->0x%03x  mt %6d%+6di v %6d%+6di metric %9u delta: %9u\n", xx | i, (xx << (SYMBITS)) | yy,
                                       metrtab[-1].re, metrtab[-1].im, vr, vi, met, met - nmetr1[0]);
#endif
#if 1
                                if (met <= metricm) {
                                        metricm = met;
                                        xmm = xx | i;
                                }
#else
				{
					unsigned int maskn = -(met <= metricm);
					unsigned int masko = ~maskn;
					metricm = (metricm & masko) | (met & maskn);
					xmm = (xmm & masko) | ((xx | i) & maskn);
				}
#endif
                        }
                        *backptr++ = xmm;
                        *nodemetric2++ = metricm;
                }
        }
}

#if defined(USEMMX) || defined(USEVIS)

static void simdinitmetric(const cplxshort_t *channel, metrictab_t *metrictab)
{
	unsigned int i1, i2, i3, i, j, k;
	int vr, vi;
	cplxshort_t a, b;

	for (i1 = 0; i1 < MLSEMTABSIZE; i1 += (2<<(SYMBITS)))
		for (i2 = 0; i2 < 2; i2++) 
			for (i3 = 0; i3 < (1<<(SYMBITS)); i3++) {
				i = i3 | (i2 << (SYMBITS)) | i1;
				a = channel[0];
				b = psk_symmapping[i & SYMBITMASK];
				vr = a.re * b.re - a.im * b.im;
				vi = a.re * b.im + a.im * b.re;
				for (k = i >> SYMBITS, j = 0; j < MLSENRSYMB; j++, k >>= SYMBITS) {
					a = channel[MLSENRSYMB-j];
					b = psk_symmapping[k & SYMBITMASK];
					vr += a.re * b.re - a.im * b.im;
					vi += a.re * b.im + a.im * b.re;
				}
				vr >>= 15;
				vi >>= 15;
				if (abs(vr) > 32767 || abs(vi) > 32767)
					logprintf(0, "initmetric: metric overflow table %u value %d%+di\n", i, vr, vi);
				metrictab->simdm[i2 | (i3 << 1) | i1] = (((int)vr) & 0xffff) | (((int)vi) << 16);
	}
}

#else

extern inline void simdinitmetric(const cplxshort_t *channel, metrictab_t *metrictab)
{
}

#endif

void pskmlse_initmetric(const cplxshort_t *channel, metrictab_t *metrictab)
{
        unsigned int i, j, k;
        int vr, vi;
        cplxshort_t a, b;

	if (checksimd()) {
		simdinitmetric(channel, metrictab);
		return;
	}
        for (i = 0; i < MLSEMTABSIZE; i++) {
                a = channel[0];
                b = psk_symmapping[i & SYMBITMASK];
                vr = a.re * b.re - a.im * b.im;
                vi = a.re * b.im + a.im * b.re;
                for (k = i >> SYMBITS, j = 0; j < MLSENRSYMB; j++, k >>= SYMBITS) {
                        a = channel[MLSENRSYMB-j];
                        b = psk_symmapping[k & SYMBITMASK];
                        vr += a.re * b.re - a.im * b.im;
                        vi += a.re * b.im + a.im * b.re;
                }
                vr >>= 15;
                vi >>= 15;
                if (abs(vr) > 32767 || abs(vi) > 32767)
                        logprintf(0, "initmetric: metric overflow table %u value %d%+di\n", i, vr, vi);
                metrictab->m[i].re = vr;
                metrictab->m[i].im = vi;
        }
}

/* ---------------------------------------------------------------------- */
