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

#include <math.h>
#include <string.h>
#include "mat.h"


/*
 * Golub/van Loan, 3.1.3, p 112; PA=LU factorization with partial pivoting
 */

#define exch(x,y)  do { float z; z = (x); (x) = (y); (y) = z; } while (0)


void frlufact(float *u, unsigned int *p, const float *a, unsigned int d)
{
	unsigned int i, j, k, mu;
	float f1, f2;

	if (u != a)
		memcpy(u, a, d*d*sizeof(u[0]));
	for (k = 0; k < d-1; k++) {
		/* search pivot index */
		for (f1 = 0, i = mu = k; i < d; i++) {
			f2 = fabs(u[i*d+k]);
			if (f2 > f1) {
				f1 = f2;
				mu = i;
			}
		}
		/* exchange rows */
		p[k] = mu;
		for (i = k; i < d; i++)
			exch(u[k*d+i], u[mu*d+i]);
		f1 = u[k*d+k];
		if (f1 != 0) {
			f1 = 1 / f1;
			for (i = k+1; i < d; i++)
				u[i*d+k] *= f1;
			for (i = k+1; i < d; i++)
				for (j = k+1; j < d; j++)
					u[i*d+j] -= u[i*d+k] * u[k*d+j];
		}
	}
}

void frlusolve(float *x, const float *b, const float *u, const unsigned int *p, unsigned int d)
{
	float *y, s;
	unsigned int k, i;

	y = alloca(d * sizeof(y[0]));
	memcpy(y, b, d * sizeof(y[0]));
	for (k = 0; k < d-1; k++) {
		i = p[k];
		if (i != k)
			exch(y[k], y[i]);
		if (y[k] == 0)
			continue;
		for (i = k+1; i < d; i++)
			y[i] -= y[k] * u[i*d+k];
	}
	/* solve Ux=y */
	for (k = d; k > 0; k--) {
		s = y[k-1];
		for (i = k; i < d; i++)
			s -= u[(k-1)*d+i] * x[i];
		x[k-1] = s / u[(k-1)*d+(k-1)];
	}
}

void frinv(float *ainv, const float *a, unsigned d)
{
	float *u, *y;
	unsigned int *p;
	unsigned int k;

	u = alloca(d * d * sizeof(u[0]));
	p = alloca((d-1) * sizeof(p[0]));
	frlufact(u, p, a, d);
	for (k = 0; k < d; k++) {
		y = &ainv[k*d];
		memset(y, 0, d * sizeof(y[0]));
		y[k] = 1;
		frlusolve(y, y, u, p, d);
	}
	frtranspose(ainv, ainv, d, d);
}

