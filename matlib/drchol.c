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
 * A el R^{d x d}
 * This routine calculates G*G^T = A, where G is lower triangular, and then uses this to solve
 * A*c=b for c
 * G*G^T*c=b
 * G*t=b
 * G^T*c=t
 */

int drcholfactor(const double *a, double *g, unsigned int d)
{
	unsigned int i, j, k;
	double s, s1;
	
	memset(g, 0, d*d*sizeof(g[0]));
	for (i = 0; i < d; i++) {
		s = a[i*d+i];
		for (j = 0; j < i; j++)
			s -= g[i*d+j] * g[i*d+j];
		if (s <= 0) {
			fprintf(stderr, "frcholfactor: matrix not positive definite a[%u][%u]=%g s=%g\n", i, i, a[i*d+i], s);
			return -1;
		}
		s = 1/sqrt(s);
		g[i*d+i] = s;
		for (j = i+1; j < d; j++) {
			s1 = 0;
			for (k = 0; k < i; k++)
				s1 += g[j*d+k] * g[i*d+k];
			g[j*d+i] = s * (a[j*d+i] - s1);
		}
	}
	return 0;
}

void drcholapply(const double *g, const double *b, double *c, unsigned int d)
{
	double *t;
	unsigned int i, j;
	double s1;

	t = alloca(d*sizeof(t[0]));
	for (i = 0; i < d; i++) {
		s1 = b[i];
		for (j = 0; j < i; j++)
			s1 -= g[i*d+j] * t[j];
		t[i] = s1 * g[i*d+i];
	}
	for (i = d; i > 0; i--) {
		s1 = t[i-1];
		for (j = i; j < d; j++)
			s1 -= g[j*d+(i-1)] * c[j];
		c[i-1] = s1 * g[(i-1)*d+(i-1)];
	}
}

int drchol(const double *a, const double *b, double *c, unsigned int d)
{
	double *g;

	g = alloca(d*d*sizeof(g[0]));
	if (drcholfactor(a, g, d)) {
		memset(c, 0, d*sizeof(c[0]));
		return -1;
	}
	drcholapply(g, b, c, d);
	return 0;
}
