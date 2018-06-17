/*****************************************************************************/

/*
 *      genpsktbl.c  --  Channel simulator for the PSK channel.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "modem.h"
#include "psk.h"
#include "mat.h"

#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------- */

/* 
 * Maximum length shift register connections
 * (cf. Proakis, Digital Communications, p. 399
 *
 *  2  1,2
 *  3  1,3
 *  4  1,4
 *  5  1,4
 *  6  1,6
 *  7  1,7
 *  8  1,5,6,7
 *  9  1,6
 * 10  1,8
 * 11  1,10
 * 12  1,7,9,12
 * 13  1,10,11,13
 * 14  1,5,9,14
 * 15  1,15
 * 16  1,5,14,16
 * 17  1,15
 * 18  1,12
 */

#define TAP_2 ((1<<1)|(1<<0))
#define TAP_3 ((1<<2)|(1<<0))
#define TAP_4 ((1<<3)|(1<<0))
#define TAP_5 ((1<<4)|(1<<1))
#define TAP_6 ((1<<5)|(1<<0))
#define TAP_7 ((1<<6)|(1<<0))
#define TAP_8 ((1<<7)|(1<<3)|(1<<2)|(1<<1))
#define TAP_9 ((1<<8)|(1<<3))
#define TAP_10 ((1<<9)|(1<<2))
#define TAP_11 ((1<<10)|(1<<1))
#define TAP_12 ((1<<11)|(1<<5)|(1<<3)|(1<<0))
#define TAP_13 ((1<<12)|(1<<3)|(1<<2)|(1<<0))
#define TAP_14 ((1<<13)|(1<<9)|(1<<5)|(1<<0))
#define TAP_15 ((1<<14)|(1<<0))
#define TAP_16 ((1<<15)|(1<<11)|(1<<2)|(1<<0))
#define TAP_17 ((1<<16)|(1<<2))
#define TAP_18 ((1<<17)|(1<<6))

#define MASK_2 ((1<<2)-1)
#define MASK_3 ((1<<3)-1)
#define MASK_4 ((1<<4)-1)
#define MASK_5 ((1<<5)-1)
#define MASK_6 ((1<<6)-1)
#define MASK_7 ((1<<7)-1)
#define MASK_8 ((1<<8)-1)
#define MASK_9 ((1<<9)-1)
#define MASK_10 ((1<<10)-1)
#define MASK_11 ((1<<11)-1)
#define MASK_12 ((1<<12)-1)
#define MASK_13 ((1<<13)-1)
#define MASK_14 ((1<<14)-1)
#define MASK_15 ((1<<15)-1)
#define MASK_16 ((1<<16)-1)
#define MASK_17 ((1<<17)-1)
#define MASK_18 ((1<<18)-1)

#define TAPS TAP_12
#define MASK MASK_12

/* ---------------------------------------------------------------------- */

extern __inline__ unsigned int hweight32(unsigned int w)
{
        unsigned int res = (w & 0x55555555) + ((w >> 1) & 0x55555555);
        res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
        res = (res & 0x0F0F0F0F) + ((res >> 4) & 0x0F0F0F0F);
        res = (res & 0x00FF00FF) + ((res >> 8) & 0x00FF00FF);
        return (res & 0x0000FFFF) + ((res >> 16) & 0x0000FFFF);
}

/* ---------------------------------------------------------------------- */

static int fcmatprintf(FILE *f, const char *name, unsigned int size1, unsigned int stride1,
		       unsigned int size2, unsigned int stride2, const cplxfloat_t *m)
{
	unsigned int i, j;
	int ret = 0;

	fprintf(f, "%s = [", name);
	for (i = 0; i < size1; i++) {
		for (j = 0; j < size2; j++) {
			ret += fprintf(f, " %g", real(m[i*stride1 + j*stride2]));
			if (imag(m[i*stride1 + j*stride2]) != 0)
				ret += fprintf(f, "%+gi", imag(m[i*stride1 + j*stride2]));
		}
		if (i+1 < size1)
			ret += fprintf(f, " ; ...\n    ");
	}
	ret += fprintf(f, " ];\n");
	return ret;
}

static inline unsigned int inttogray(unsigned int x)
{
	unsigned int y, r = 0;
	
	for (y = 1; y <= 0x80; y <<= 1)
		r |= ((x+y)>>1) & y;
	return r;
}

/* ---------------------------------------------------------------------- */

static unsigned int rndseed = 1;

unsigned int random_num(void)
{
        unsigned int tmp2;
        unsigned long long tmp;

        if (!rndseed)
                return (rndseed = 1);
        tmp = rndseed * 16807ULL;
        tmp2 = (tmp & 0x7FFFFFFFU) + (tmp >> 31);
        if (tmp2 >= 0x7FFFFFFFU)
                tmp2 -= 0x7FFFFFFFU;
        return (rndseed = tmp2);
}

static void findtrainseq2(FILE *f, int trsymseq[TRAINSYMS], cplxint_t symmapping[(1<<(SYMBITS))])
{
	cplxfloat_t symmap[(1<<(SYMBITS))], ct;
	int trsym[2*TRAINSYMS];
	cplxfloat_t ma[OBSTRAINSYMS*CHANNELLEN], mah[CHANNELLEN*OBSTRAINSYMS], maha[CHANNELLEN*CHANNELLEN];
	float odmax, odmaxmin = FLT_MAX, ft;
	unsigned int cnt, i, j;

	/* first calculate symbol mapping */
	for (i = 0; i < (1<<(SYMBITS)); i++) {
		symmap[i].re = symmapping[i].re * (1.0 / 32767);
		symmap[i].im = symmapping[i].im * (1.0 / 32767);
	}
	for (cnt = 0; cnt < 20000; cnt++) {
		for (i = 0; i < TRAINSYMS; i++)
			trsym[i+TRAINSYMS] = trsym[i] = random_num() & SYMBITMASK;
		for (i = 0; i < OBSTRAINSYMS; i++)
			for (j = 0; j < CHANNELLEN; j++)
				ma[i*CHANNELLEN+j] = symmap[trsym[i+j]];
		fchermtranspose(mah, ma, OBSTRAINSYMS, CHANNELLEN);
		fcmul(maha, mah, ma, CHANNELLEN, OBSTRAINSYMS, CHANNELLEN);
		//fcmatprintf(f, "a", OBSTRAINSYMS, CHANNELLEN, CHANNELLEN, 1, ma);
		//fcmatprintf(f, "aha", CHANNELLEN, CHANNELLEN, CHANNELLEN, 1, maha);
		odmax = 0;
		for (i = 0; i < CHANNELLEN; i++)
			for (j = 0; j < i; j++) {
				ct = maha[i*CHANNELLEN+j];
				ft = ct.re * ct.re + ct.im * ct.im;
				if (ft > odmax)
					odmax = ft;
			}
		if (odmax < odmaxmin) {
			odmaxmin = odmax;
			memcpy(trsymseq, trsym, TRAINSYMS*sizeof(trsymseq[0]));
		}
		//fprintf(f, "/* iter %3d offdiag max: %g maxmin: %g */\n", cnt, odmax, odmaxmin);
	}
	fprintf(f, "/* train sequence: min max offdiag a*a^H: %g */\n", odmaxmin);
}

/* ---------------------------------------------------------------------- */

static void findtrainseq(FILE *f, int trsymseq[TRAINSYMS], cplxint_t symmapping[(1<<(SYMBITS))])
{
	unsigned int pn = 0xaaaaaaaa;
	unsigned int i, j, k, new;

	for (i = 0; i < TRAINSYMS; i++) {
		for (k = j = 0; j < SYMBITS; j++) {
			new = hweight32(pn & TAPS) & 1;
			pn = ((pn << 1) | new) & MASK;
			k = (k << 1) | new;
		}
		trsymseq[i] = k;
	}
}

/* ---------------------------------------------------------------------- */

static void gentrain(FILE *f)
{
	int trsym[2*TRAINSYMS];
	float sumr, sumi, expr, expi, vr, vi;
	unsigned int i, j, k;
	cplxfloat_t ma[OBSTRAINSYMS*CHANNELLEN], mah[CHANNELLEN*OBSTRAINSYMS], maha[CHANNELLEN*CHANNELLEN];
	cplxfloat_t mahainv[CHANNELLEN*CHANNELLEN], mahainvah[CHANNELLEN*OBSTRAINSYMS];
	double ph;
	cplxint_t symmapping[(1<<(SYMBITS))];

	/* first calculate symbol mapping */
	for (i = 0; i < (1<<(SYMBITS)); i++) {
		j = inttogray(i) & ((1<<(SYMBITS))-1);
		ph = i * (2.0 * M_PI / (1<<(SYMBITS)));
		symmapping[j].re = 32767 * cos(ph);
		symmapping[j].im = 32767 * sin(ph);
		fprintf(f, "/* %02x  %02x   %6d%+6di */\n", i, j, symmapping[j].re, symmapping[j].im);
	}
	findtrainseq2(f, trsym, symmapping);
	/*memcpy(&trsym[TRAINSYMS], trsym, sizeof(trsym)/2);*/
	fprintf(f, "\nconst cplxshort_t psk_symmapping[%u] = {", (1<<(SYMBITS)));
	for (i = 0; ; i++) {
		fprintf(f, "\n\t{ %d, %d }", symmapping[i].re, symmapping[i].im);
		if (i >= (1<<(SYMBITS))-1)
			break;
		fprintf(f, ",");
	}
	fprintf(f, "\n};\n\n");
	/* calc training symbols */
	fprintf(f, "/*\n  trsym = [");
	for (i = 0; i < TRAINSYMS; i++) {
		trsym[i+TRAINSYMS] = trsym[i];
		fprintf(f, " %d%+di", symmapping[trsym[i]].re, symmapping[trsym[i]].im);
	}
	fprintf(f, " ];\n  trsymcc = [");
	for (i = 0; i < TRAINSYMS; i++) {
		for (sumr = sumi = 0, j = 0; j < TRAINSYMS; j++) {
			sumr += symmapping[trsym[j]].re * symmapping[trsym[i+j]].re +
				symmapping[trsym[j]].im * symmapping[trsym[i+j]].im;
			sumi += symmapping[trsym[j]].im * symmapping[trsym[i+j]].re -
				symmapping[trsym[j]].re * symmapping[trsym[i+j]].im;
		}
		fprintf(f, " %g%+gi", sumr * (1.0 / 32768 / 32768), sumi * (1.0 / 32768 / 32768));
	}
	fprintf(f, " ];\n");
	for (i = 0; i < OBSTRAINSYMS; i++)
		for (j = 0; j < CHANNELLEN; j++) {
			ma[i*CHANNELLEN+j].re = symmapping[trsym[i+j]].re * (1.0 / 32768);
			ma[i*CHANNELLEN+j].im = symmapping[trsym[i+j]].im * (1.0 / 32768);
		}
	fcmatprintf(f, "a", OBSTRAINSYMS, CHANNELLEN, CHANNELLEN, 1, ma);
	/* transpose it */
	fchermtranspose(mah, ma, OBSTRAINSYMS, CHANNELLEN);
	fcmul(maha, mah, ma, CHANNELLEN, OBSTRAINSYMS, CHANNELLEN);
	fcinv(mahainv, maha, CHANNELLEN);
	fcmul(mahainvah, mahainv, mah, CHANNELLEN, CHANNELLEN, OBSTRAINSYMS);
	fcmatprintf(f, "ahainvah", CHANNELLEN, OBSTRAINSYMS, OBSTRAINSYMS, 1, mahainvah);
	fprintf(f, " */\n\n");
	/* generate C code */
	fprintf(f, "\nstatic const unsigned char trainsyms[%u] = {\n\t", TRAINSYMS);
	for (i = 0;;) {
		fprintf(f, "%d", trsym[i]);
		if ((++i) >= TRAINSYMS)
			break;
		if (!(i & 15)) {
			fprintf(f, ",\n\t");
			continue;
		}
		fprintf(f, ", ");
	}
#if 0
	fprintf(f, "\n};\n\nstatic const cplxshort_t traincorr[%u] = {\n", TRAINSYMS);
	for (i = 0;;) {
		if (!(i & 3))
			fprintf(f, "\n\t");
		fprintf(f, "{ %d, %d }", symmapping[trsym[i]].re, -symmapping[trsym[i]].im);
		if (i >= (TRAINSYMS-1))
			break;
		i++;
		fprintf(f, ", ");
	}
#endif
	fprintf(f, "\n};\n\nstatic const cplxshort_t traincorrrotated[%u] = {\n", TRAINSYMS);
	ph = 2.0 * M_PI * FCARRIER / SYMRATE;
	for (i = 0;;) {
		if (!(i & 3))
			fprintf(f, "\n\t");
		sumr = symmapping[trsym[i]].re;
		sumi = -symmapping[trsym[i]].im;
		expr = cos(ph * i);
		expi = -sin(ph * i);
		vr = sumr * expr - sumi * expi;
		vi = sumr * expi + sumi * expr;
		fprintf(f, "{ %d, %d }", (int)vr, (int)vi);
		if (i >= (TRAINSYMS-1))
			break;
		i++;
		fprintf(f, ", ");
	}
#if 0
	fprintf(f, "\n};\n\nstatic const cplxshort_t trainmat[%u] = {\n", CHANNELLEN*OBSTRAINSYMS);
	for (i = 0; i < CHANNELLEN; i++) {
		fprintf(f, "\t");
		for (j = 0; j < OBSTRAINSYMS; j++) {
			fprintf(f, "{ %d, %d }", (int)((float)(1 << 16) * mahainvah[i*OBSTRAINSYMS+j].re),
				(int)((float)(1 << 16) * mahainvah[i*OBSTRAINSYMS+j].im));
			if (i == CHANNELLEN-1 && j == OBSTRAINSYMS-1)
				continue;
			if ((j & 7) == 7 && j != OBSTRAINSYMS-1) {
				fprintf(f, ",\n\t");
				continue;
			}
			fprintf(f, ", ");
		}
		fprintf(f, "\n");
	}
#endif
	fprintf(f, "\n};\n\nstatic const cplxshort_t trainmatrotated[%u] = {\n", CHANNELLEN*OBSTRAINSYMS);
	ph = -2.0 * M_PI * FCARRIER / SYMRATE;
	for (i = 0; i < CHANNELLEN; i++) {
		fprintf(f, "\t");
		for (j = 0; j < OBSTRAINSYMS; j++) {
			k = i*OBSTRAINSYMS+j;
			sumr = (float)(1 << 16) * mahainvah[k].re;
			sumi = (float)(1 << 16) * mahainvah[k].im;
			expr = cos(ph * (OBSTRAINSYMS-j));
			expi = -sin(ph * (OBSTRAINSYMS-j));
			vr = sumr * expr - sumi * expi;
			vi = sumr * expi + sumi * expr;
			fprintf(f, "{ %d, %d }", (int)(vr), (int)(vi));
			if (i == CHANNELLEN-1 && j == OBSTRAINSYMS-1)
				continue;
			if ((j & 7) == 7 && j != OBSTRAINSYMS-1) {
				fprintf(f, ",\n\t");
				continue;
			}
			fprintf(f, ", ");
		}
		fprintf(f, "\n");
	}
	fprintf(f, "};\n\n");
	/* calculate MLSE root and toor node */
	for (i = j = 0; i < CHANNELLEN-1; i++)
		j |= trsym[TRAINSYMS-1-i] << (i * SYMBITS);
	fprintf(f, "#define MLSEROOTNODE  0x%x\n", j);
	for (i = j = 0; i < CHANNELLEN-1; i++)
		j |= trsym[CHANNELLEN-2-i] << (i * SYMBITS);
	fprintf(f, "#define MLSETOORNODE  0x%x\n\n", j);
}

#define TXRCOSALPHA 0.4
#define RXRCOSALPHA 0.4

#define TXFILTERRELAX 1.4
#define RXFILTERRELAX 1.4

static inline double sinc(double x)
{
        double arg = x * M_PI;

 	if (fabs(arg) < 1e-10)
                return 1;
        return sin(arg) / arg;
}

static inline double hamming(double x)
{
        return 0.54-0.46*cos((2*M_PI)*x);
}

static void gentxfilt(FILE *f)
{
	float coeff[TXFILTLEN * TXFILTOVER];
	double tmul, at, t, f1, f2;
	int i, j;

	tmul = 1.0 / TXFILTOVER;
#if 0
        tmul *= TXFILTERRELAX;
        for (i = 0; i < TXFILTLEN * TXFILTOVER; i++)
                coeff[i] = sinc((i - 0.5 * TXFILTLEN * TXFILTOVER)*tmul)
                        * hamming(i * (1.0 / (TXFILTLEN * TXFILTOVER - 1)));
#else
        for (i = 0; i < TXFILTLEN * TXFILTOVER; i++) {
                t = (i - 0.5 * TXFILTLEN * TXFILTOVER) * tmul;
                at = t * TXRCOSALPHA;
                f1 = 1 - 4 * at * at;
                if (fabs(f1) < 1e-10)
                        f2 = M_PI * (1.0 / 8.0) * sin(M_PI * at) / at;
                else
                        f2 = cos(M_PI * at) / f1;
                coeff[i] = sinc(t) * f2;
        }
#endif
        for (f1 = 0, i = 0; i < TXFILTOVER; i++) {
		for (f2 = 0, j = 0; j < TXFILTLEN; j++)
			f2 += fabs(coeff[j*TXFILTOVER+i]);
		if (f2 > f1)
			f1 = f2;
	}
	f2 = 32767 / f1;
	fprintf(f, "/*\n  txfilt = [");
        for (i = 0;;) {
		fprintf(f, " %g ;", coeff[i]);
		if ((++i) >= TXFILTLEN * TXFILTOVER)
			break;
		if (i & 3)
			continue;
		fprintf(f, "\n            ");
	}
	fprintf(f, " ];\n  abssum = %g;\n  semilogy((0:%u)/%u,abs(fft(txfilt)))\n */\n\n", 
		f1, TXFILTLEN * TXFILTOVER - 1, TXFILTLEN * TXFILTOVER);
	/* output C array with filter coefficients */
	fprintf(f, "static const int txfilter[%u][%u] = {", TXFILTOVER, TXFILTLEN);
        for (i = 0;;) {
		fprintf(f, "\n\t{");
		for (j = 0;;) {
			fprintf(f, " %d", (int)(f2 * coeff[j*TXFILTOVER+i]));
			if ((++j) >= TXFILTLEN)
				break;
			fprintf(f, ",");
		}
		fprintf(f, " }");
		if ((++i) >= TXFILTOVER)
			break;
		fprintf(f, ",");		
	}
	fprintf(f, "\n};\n\n");
}

static void genrxfilt(FILE *f)
{
	float coeff[RXFILTLEN * RXFILTOVER];
	double tmul, at, t, f1, f2;
	int i, j, k;

	tmul = 1.0 * SYMRATE / RXFILTOVER / SAMPLERATE;
#if 1
        tmul *= RXFILTERRELAX;
        for (i = 0; i < RXFILTLEN * RXFILTOVER; i++)
                coeff[i] = sinc((i - 0.5 * RXFILTLEN * RXFILTOVER)*tmul)
                        * hamming(i * (1.0 / (RXFILTLEN * RXFILTOVER - 1)));
#else
        for (i = 0; i < RXFILTLEN * RXFILTOVER; i++) {
                t = (i - 0.5 * RXFILTLEN * RXFILTOVER) * tmul;
                at = t * RXRCOSALPHA;
                f1 = 1 - 4 * at * at;
                if (fabs(f1) < 1e-10)
                        f2 = M_PI * (1.0 / 8.0) * sin(M_PI * at) / at;
                else
                        f2 = cos(M_PI * at) / f1;
                coeff[i] = sinc(t) * f2;
        }
#endif
        for (f1 = 0, i = 0; i < RXFILTOVER; i++) {
		for (f2 = 0, j = 0; j < RXFILTLEN; j++)
			f2 += fabs(coeff[j*RXFILTOVER+i]);
		if (f2 > f1)
			f1 = f2;
	}
	f2 = 65535 / f1;
	fprintf(f, "/*\n  rxfilt = [");
        for (i = 0;;) {
		fprintf(f, " %g ;", coeff[i]);
		if ((++i) >= RXFILTLEN * RXFILTOVER)
			break;
		if (i & 3)
			continue;
		fprintf(f, "\n            ");
	}
	fprintf(f, " ];\n  abssum = %g;\n  semilogy((0:%u)/%u,abs(fft(rxfilt)))\n */\n\n", 
		f1, RXFILTLEN * RXFILTOVER - 1, RXFILTLEN * RXFILTOVER);
	/* output C array with filter coefficients */
#if 0
	fprintf(f, "static const int rxfilter[%u][%u] = {", RXFILTOVER, RXFILTLEN);
        for (i = 0;;) {
		fprintf(f, "\n\t{");
		for (j = 0;;) {
			fprintf(f, " %d", (int)(f2 * coeff[j*RXFILTOVER+i]));
			if ((++j) >= RXFILTLEN)
				break;
			fprintf(f, ",");
		}
		fprintf(f, " }");
		if ((++i) >= RXFILTOVER)
			break;
		fprintf(f, ",");		
	}
	fprintf(f, "\n};\n\n");
#endif
	/* output C arrays with filter coefficients and downmixing combined */
	tmul = -2.0 * M_PI * FCARRIER / RXFILTOVER / SAMPLERATE;
	/* warning: filter runs backwards! */
	fprintf(f, "static const int rxfilter_re[%u][%u] = {", RXFILTOVER, RXFILTLEN);
        for (i = RXFILTOVER-1;;) {
		fprintf(f, "\n\t{");
		for (j = 0;;) {
			k = j*RXFILTOVER+i;
			fprintf(f, " %d", (int)(f2 * coeff[k] * cos(tmul * k)));
			if ((++j) >= RXFILTLEN)
				break;
			fprintf(f, ",");
		}
		fprintf(f, " }");
		if (!(i--))
			break;
		fprintf(f, ",");		
	}
	fprintf(f, "\n};\n\nstatic const int rxfilter_im[%u][%u] = {", RXFILTOVER, RXFILTLEN);
        for (i = RXFILTOVER-1;;) {
		fprintf(f, "\n\t{");
		for (j = 0;;) {
			k = j*RXFILTOVER+i;
			fprintf(f, " %d", (int)(f2 * coeff[k] * sin(tmul * k)));
			if ((++j) >= RXFILTLEN)
				break;
			fprintf(f, ",");
		}
		fprintf(f, " }");
		if (!(i--))
			break;
		fprintf(f, ",");		
	}
	fprintf(f, "\n};\n\n");
	/* output C arrays with filter coefficients and downmixing combined; int16 format (useful for mmx) */
	tmul = -2.0 * M_PI * FCARRIER / RXFILTOVER / SAMPLERATE;
	/* warning: filter runs backwards! */
	fprintf(f, "static const int16_t rxfilter_re_16[%u][%u] = {", RXFILTOVER, RXFILTLEN);
        for (i = RXFILTOVER-1;;) {
		fprintf(f, "\n\t{");
		for (j = 0;;) {
			k = j*RXFILTOVER+i;
			fprintf(f, " %d", (int)(f2 * coeff[k] * cos(tmul * k)));
			if ((++j) >= RXFILTLEN)
				break;
			fprintf(f, ",");
		}
		fprintf(f, " }");
		if (!(i--))
			break;
		fprintf(f, ",");
	}
	fprintf(f, "\n};\n\nstatic const int16_t rxfilter_im_16[%u][%u] = {", RXFILTOVER, RXFILTLEN);
        for (i = RXFILTOVER-1;;) {
		fprintf(f, "\n\t{");
		for (j = 0;;) {
			k = j*RXFILTOVER+i;
			fprintf(f, " %d", (int)(f2 * coeff[k] * sin(tmul * k)));
			if ((++j) >= RXFILTLEN)
				break;
			fprintf(f, ",");
		}
		fprintf(f, " }");
		if (!(i--))
			break;
		fprintf(f, ",");
	}
	fprintf(f, "\n};\n\n");
}

#define COSBITS 9

static void gencos(FILE *f)
{
	unsigned int i;

	fprintf(f, "static const short costab[%u] = {", (1<<(COSBITS)));
	for (i = 0; ; i++) {
		if (!(i & 7))
			fprintf(f, "\n\t");
		fprintf(f, "%6d", (int)(32767*cos(i * (2.0 * M_PI / (1<<(COSBITS))))));
		if (i >= (1<<(COSBITS))-1)
			break;
		fprintf(f, ", ");
	}
	fprintf(f, "\n};\n\n#define COS(x) (costab[(((x)>>%u)&0x%x)])\n#define SIN(x) COS((x)+0xc000)\n\n",
		16-COSBITS, (1<<(COSBITS))-1);
}


/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	gentrain(stdout);
	gentxfilt(stdout);
	genrxfilt(stdout);
	gencos(stdout);
	return 0;
}
