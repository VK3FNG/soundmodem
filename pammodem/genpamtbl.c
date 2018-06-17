/*****************************************************************************/

/*
 *      genpamtbl.c  --  Channel simulator for the PAM channel.
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

#include "pam.h"

#include "mat.h"

#include <math.h>
#include <stdio.h>

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

#define TAPS TAP_5
#define MASK MASK_5

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

static int frmatprintf(FILE *f, const char *name, unsigned int size1, unsigned int stride1,
                       unsigned int size2, unsigned int stride2, const float *m)
{
        unsigned int i, j;
        int ret = 0;

        fprintf(f, "%s = [", name);
        for (i = 0; i < size1; i++) {
                for (j = 0; j < size2; j++)
                        ret += fprintf(f, " %g", m[i*stride1 + j*stride2]);
                if (i+1 < size1)
                        ret += fprintf(f, " ;\n    ");
        }
        ret += fprintf(f, " ];\n");
        return ret;
}

static void gentrain(FILE *f)
{
	unsigned int pn = 0xaaaaaaaa;
	int trsym[2*TRAINBITS];
	int sum;
	unsigned int i, j, new;
	float ma[OBSTRAINBITS*CHANNELLEN], mat[CHANNELLEN*OBSTRAINBITS], mata[CHANNELLEN*CHANNELLEN];
	float matainv[CHANNELLEN*CHANNELLEN], matainvat[CHANNELLEN*OBSTRAINBITS];

	fprintf(f, "/*\n  trsym = [");
	for (sum = i = 0; i < TRAINBITS; i++) {
		new = hweight32(pn & TAPS) & 1;
		pn = ((pn << 1) | new) & MASK;
		if (i == TRAINBITS-1)
			new = sum < 0;
		trsym[i+TRAINBITS] = trsym[i] = -((-new) | 1);
		sum += trsym[i];
		fprintf(f, " %d", trsym[i]);
	}
	fprintf(f, " ];\n  trsymcc = [");
	for (i = 0; i < TRAINBITS; i++) {
		for (sum = 0, j = 0; j < TRAINBITS; j++)
			sum += trsym[j] * trsym[i+j];
		fprintf(f, " %d", sum);
	}
	fprintf(f, " ];\n");
	for (i = 0; i < OBSTRAINBITS; i++)
		for (j = 0; j < CHANNELLEN; j++)
			ma[i*CHANNELLEN+j] = trsym[i+j];
	frmatprintf(f, "a", OBSTRAINBITS, CHANNELLEN, CHANNELLEN, 1, ma);
	/* transpose it */
	frtranspose(mat, ma, OBSTRAINBITS, CHANNELLEN);
	frmul(mata, mat, ma, CHANNELLEN, OBSTRAINBITS, CHANNELLEN);
	frinv(matainv, mata, CHANNELLEN);
	frmul(matainvat, matainv, mat, CHANNELLEN, CHANNELLEN, OBSTRAINBITS);
	frmatprintf(f, "atainvat", CHANNELLEN, OBSTRAINBITS, OBSTRAINBITS, 1, matainvat);
	fprintf(f, " */\n\n");
	/* generate C code */
	fprintf(f, "static const unsigned char trainsymbmap[%u] = {\n\t", (TRAINBITS+7) / 8);
	for (i = 0;;) {
		for (new = j = 0; j < 8; j++)
			if (trsym[i+j] > 0)
				new |= 1 << j;
		fprintf(f, "0x%02x", new);
		i += 8;
		if (i >= TRAINBITS)
			break;
		fprintf(f, ", ");
	}
	fprintf(f, "\n};\n\nstatic const int trainsyms[%u] = {\n\t", TRAINBITS);
	for (i = 0;;) {
		fprintf(f, "%d", trsym[i]);
		if ((++i) >= TRAINBITS)
			break;
		if (!(i & 15)) {
			fprintf(f, ",\n\t");
			continue;
		}
		fprintf(f, ", ");
	}
	fprintf(f, "\n};\n\nstatic const int trainmat[%u] = {\n", CHANNELLEN*OBSTRAINBITS);
	for (i = 0; i < CHANNELLEN; i++) {
		fprintf(f, "\t");
		for (j = 0; j < OBSTRAINBITS; j++) {
			fprintf(f, "%d", (int)((1 << 16) * matainvat[i*OBSTRAINBITS+j]));
			if (i == CHANNELLEN-1 && j == OBSTRAINBITS-1)
				continue;
			if ((j & 7) == 7 && j != OBSTRAINBITS-1) {
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
		if (trsym[TRAINBITS-1-i] > 0)
			j |= 1 << i;
	fprintf(f, "#define MLSEROOTNODE  0x%x\n", j);
	for (i = j = 0; i < CHANNELLEN-1; i++)
		if (trsym[CHANNELLEN-2-i] > 0)
			j |= 1 << i;
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
	fprintf(f, " ];\n  abssum = %g;\n  semilogy((0:%u)/%u,abs(fft(txfilt)))\n */\n\nstatic const int txfilter[%u][%u] = {", 
		f1, TXFILTLEN * TXFILTOVER - 1, TXFILTLEN * TXFILTOVER, TXFILTOVER, TXFILTLEN);
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
	int i, j;

	tmul = 1.0 * BITRATE / RXFILTOVER / SAMPLERATE;
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
	fprintf(f, " ];\n  abssum = %g;\n  semilogy((0:%u)/%u,abs(fft(rxfilt)))\n */\n\nstatic const int rxfilter[%u][%u] = {", 
		f1, RXFILTLEN * RXFILTOVER - 1, RXFILTLEN * RXFILTOVER, RXFILTOVER, RXFILTLEN);
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
}


/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	gentrain(stdout);
	gentxfilt(stdout);
	genrxfilt(stdout);
	return 0;
}
