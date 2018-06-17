/*
 * Matrix operations library
 *
 * Copyright (C) 1999-2000
 *   Thomas Sailer, <sailer@ife.ee.ethz.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Test matrix routines */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "mat.h"

static int frmatprintf(const char *name, unsigned int size1, unsigned int stride1,
		       unsigned int size2, unsigned int stride2, const float *m)
{
	unsigned int i, j;
	int ret = 0;

	fprintf(stdout, "%s = [", name);
	for (i = 0; i < size1; i++) {
		for (j = 0; j < size2; j++)
			ret += fprintf(stdout, " %g", m[i*stride1 + j*stride2]);
		if (i+1 < size1)
			ret += fprintf(stdout, " ; ...\n    ");
	}
	ret += fprintf(stdout, " ];\n");
	return ret;
}

static int fcmatprintf(const char *name, unsigned int size1, unsigned int stride1,
		       unsigned int size2, unsigned int stride2, const cplxfloat_t *m)
{
	unsigned int i, j;
	int ret = 0;

	fprintf(stdout, "%s = [", name);
	for (i = 0; i < size1; i++) {
		for (j = 0; j < size2; j++) {
			ret += fprintf(stdout, " %g", real(m[i*stride1 + j*stride2]));
			if (imag(m[i*stride1 + j*stride2]) != 0)
				ret += fprintf(stdout, "%+gi", imag(m[i*stride1 + j*stride2]));
		}
		if (i+1 < size1)
			ret += fprintf(stdout, " ; ...\n    ");
	}
	ret += fprintf(stdout, " ];\n");
	return ret;
}

static int testfrgolub(void)
{
	static const float a[9] = { 3, 17, 10, 2, 4, -2, 6, 18, -12 };
	static const float uexp[9] = { 6, 18, -12, 1.0/3, 8, 16, 1.0/2, -1.0/4, 6 };
	float u[9];
	unsigned int p[2];
	unsigned int i, err = 0;

	frlufact(u, p, a, 3);
	for (i = 0; i < 9; i++) 
		if (fabs(u[i] - uexp[i]) > 0.0001)
			err |= 1;
	if (err & 1) {
		printf("LU factorization (Golub example) error!\n");
		frmatprintf("a", 3, 3, 3, 1, a);
		frmatprintf("u", 3, 3, 3, 1, u);
		frmatprintf("uexp", 3, 3, 3, 1, uexp);
	}
	return err ? 1 : 0;
}


static int testfrinv(void)
{
	unsigned int d;
	float *a, *b, *c;
	unsigned int i, j, err = 0;

	d = (random() & 15) + 1;
	a = alloca(d * d * sizeof(a[0]));
	b = alloca(d * d * sizeof(b[0]));
	c = alloca(d * d * sizeof(c[0]));
	for (i = 0; i < d*d; i++)
		a[i] = (random() - RAND_MAX/2) * (4.0 / RAND_MAX);
	/* check normal inversion */
	frinv(b, a, d);
	frmul(c, a, b, d, d, d);
	for (i = 0; i < d; i++)
		for (j = 0; j < d; j++)
			if (((i == j) && (fabs(c[i*d+j] - 1) > 0.0001)) ||
			    ((i != j) && (fabs(c[i*d+j] - 0) > 0.0001)))
				err |= 1;
	if (err & 1) {
		printf("LU inversion error!\n");
		frmatprintf("a", d, d, d, 1, a);
		frmatprintf("b", d, d, d, 1, b);
		frmatprintf("c", d, d, d, 1, c);
	}
	return err ? 1 : 0;
}

static int testfrchol(void)
{
	unsigned int d;
	float *a, *b, *c, *g, *z;
	unsigned int i, j, err = 0;

	d = (random() & 15) + 1;
	a = alloca(d * d * sizeof(a[0]));
	b = alloca(d * d * sizeof(b[0]));
	c = alloca(d * d * sizeof(c[0]));
	g = alloca(d * d * sizeof(g[0]));
	for (i = 0; i < d; i++) {
		a[i*d+i] = (random() - RAND_MAX/2) * (4.0 / RAND_MAX);
		a[i*d+i] *= a[i*d+i];
		for (j = i+1; j < d; j++)
			a[i*d+i] += fabs(a[i*d+j] = a[j*d+i] = (random() - RAND_MAX/2) * (4.0 / RAND_MAX));
	}
	if (frcholfactor(a, g, d))
		return 1;
	for (i = 0; i < d; i++) {
		z = &b[i*d];
		memset(z, 0, d * sizeof(z[0]));
		z[i] = 1;
		frcholapply(g, z, z, d);
	}
	frtranspose(b, b, d, d);
	frmul(c, a, b, d, d, d);
	for (i = 0; i < d; i++)
		for (j = 0; j < d; j++)
			if (((i == j) && (fabs(c[i*d+j] - 1) > 0.0001)) ||
			    ((i != j) && (fabs(c[i*d+j] - 0) > 0.0001)))
				err |= 1;
	if (err & 1) {
		printf("Cholesky inversion error!\n");
		frmatprintf("a", d, d, d, 1, a);
		frmatprintf("b", d, d, d, 1, b);
		frmatprintf("c", d, d, d, 1, c);
	}
	return err ? 1 : 0;
}

static inline float cdiffsq(cplxfloat_t c, float r)
{
	real(c) -= r;
	return real(c) * real(c) + imag(c) * imag(c);
}

static int testfcinv(void)
{
	unsigned int d;
	cplxfloat_t *a, *b, *c;
	unsigned int i, j, err = 0;

	d = (random() & 15) + 1;
	a = alloca(d * d * sizeof(a[0]));
	b = alloca(d * d * sizeof(b[0]));
	c = alloca(d * d * sizeof(c[0]));
	for (i = 0; i < d*d; i++)
		cplx(a[i], (random() - RAND_MAX/2) * (4.0 / RAND_MAX),
		     (random() - RAND_MAX/2) * (4.0 / RAND_MAX));
	/* check normal inversion */
	fcinv(b, a, d);
	fcmul(c, a, b, d, d, d);
	for (i = 0; i < d; i++)
		for (j = 0; j < d; j++)
			if (((i == j) && (cdiffsq(c[i*d+j], 1) > 0.0001)) ||
			    ((i != j) && (cdiffsq(c[i*d+j], 0) > 0.0001)))
				err |= 1;
	if (err & 1) {
		printf("Complex LU inversion error!\n");
		fcmatprintf("a", d, d, d, 1, a);
		fcmatprintf("b", d, d, d, 1, b);
		fcmatprintf("c", d, d, d, 1, c);
	}
	return err ? 1 : 0;
}

static int testfcchol(void)
{
	unsigned int d;
	cplxfloat_t *a, *b, *c, *g, *z;
	float v;
	unsigned int i, j, err = 0;

	d = (random() & 15) + 1;
	a = alloca(d * d * sizeof(a[0]));
	b = alloca(d * d * sizeof(b[0]));
	c = alloca(d * d * sizeof(c[0]));
	g = alloca(d * d * sizeof(g[0]));
	for (i = 0; i < d; i++) {
		v = (random() - RAND_MAX/2) * (4.0 / RAND_MAX);
		cplx(a[i*d+i], v * v, 0); 
		for (j = i+1; j < d; j++) {
			cplx(a[i*d+j], (random() - RAND_MAX/2) * (4.0 / RAND_MAX), 
			     (random() - RAND_MAX/2) * (4.0 / RAND_MAX));
			conj(a[j*d+i], a[i*d+j]);
			real(a[i*d+i]) += fabs(real(a[i*d+j])) + fabs(imag(a[i*d+j]));
		}
	}
	if (fccholfactor(a, g, d))
		return 1;
	for (i = 0; i < d; i++) {
		z = &b[i*d];
		memset(z, 0, d * sizeof(z[0]));
		cplx(z[i], 1, 0);
		fccholapply(g, z, z, d);
	}
	fctranspose(b, b, d, d);
	fcmul(c, a, b, d, d, d);
	for (i = 0; i < d; i++)
		for (j = 0; j < d; j++)
			if (((i == j) && (cdiffsq(c[i*d+j], 1) > 0.0001)) ||
			    ((i != j) && (cdiffsq(c[i*d+j], 0) > 0.0001)))
				err |= 1;
	if (err & 1) {
		printf("Complex Cholesky inversion error!\n");
		fcmatprintf("a", d, d, d, 1, a);
		fcmatprintf("b", d, d, d, 1, b);
		fcmatprintf("c", d, d, d, 1, c);
	}
	return err ? 1 : 0;
}

int main(int argc, char *argv[])
{
	srandom(time(NULL));
	testfrgolub();
	testfrinv();
	testfrchol();
	testfcinv();
	testfcchol();
	return 0;
}
