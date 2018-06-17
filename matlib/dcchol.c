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

#include "mat.h"
#include <math.h>
#include <stdio.h>
#include <string.h>


/*
 * A el C^{d x d}
 * This routine calculates G*G^H = A, where G is lower triangular, and then uses this to solve
 * A*c=b for c
 * G*G^H*c=b
 * G*t=b
 * G^H*c=t
 */

static inline double pwr(cplxdouble_t c)
{
	return real(c) * real(c) + imag(c) * imag(c);
}

int dccholfactor(const cplxdouble_t *a, cplxdouble_t *g, unsigned int d)
{
	unsigned int i, j, k;
	cplxdouble_t sc, co;
	double s;

	memset(g, 0, d*d*sizeof(g[0]));
	for (i = 0; i < d; i++) {
		s = real(a[i*d+i]);
		for (j = 0; j < i; j++)
			s -= pwr(g[i*d+j]);
		if (s <= 0 || imag(a[i*d+i]) != 0) {
			fprintf(stderr, "dccholfactor: matrix not positive definite a[%u][%u]=%g%+gi s=%g\n", i, i, real(a[i*d+i]), imag(a[i*d+i]), s);
			return -1;
		}
		s = 1/sqrt(s);
		cplx(g[i*d+i], s, 0);
		for (j = i+1; j < d; j++) {
			sc = a[j*d+i];
			for (k = 0; k < i; k++) {
				conj(co, g[i*d+k]);
				cmsub(sc, g[j*d+k], co);
			}
			cmuls(g[j*d+i], sc, s);
		}
	}
	return 0;
}

void dccholapply(const cplxdouble_t *g, const cplxdouble_t *b, cplxdouble_t *c, unsigned int d)
{
	cplxdouble_t *t, s, s2;
	unsigned int i, j;

	t = alloca(d*sizeof(t[0]));
	for (i = 0; i < d; i++) {
		s = b[i];
		for (j = 0; j < i; j++)
			cmsub(s, g[i*d+j], t[j]);
		/* g's diagonal is real, therefore we have a division by a real */
		cmuls(t[i], s, real(g[i*d+i]));
	}
	for (i = d; i > 0; i--) {
		s = t[i-1];
		for (j = i; j < d; j++) {
			conj(s2, g[j*d+(i-1)]);
			cmsub(s, s2, c[j]);
		}
		/* g's diagonal is real, therefore we have a division by a real */
		cmuls(c[i-1], s, real(g[(i-1)*d+(i-1)]));
	}
}

int dcchol(const cplxdouble_t *a, const cplxdouble_t *b, cplxdouble_t *c, unsigned int d)
{
	cplxdouble_t *g;

	g = alloca(d*d*sizeof(g[0]));
	if (dccholfactor(a, g, d)) {
		memset(c, 0, d*sizeof(c[0]));
		return -1;
	}
	dccholapply(g, b, c, d);
	return 0;
}
