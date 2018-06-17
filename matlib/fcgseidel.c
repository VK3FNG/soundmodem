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

/*
 * This routine calculates A*c=b iteratively, using the Gauss-Seidel iteration method
 */

int fcgaussseidel(const cplxfloat_t *a, const cplxfloat_t *b, cplxfloat_t *c, unsigned int d, unsigned int iter)
{
	cplxfloat_t *adiag;
	unsigned int i, j, k;
	cplxfloat_t s;

	adiag = alloca(d*sizeof(adiag[0]));
	/* initial vector */
	for (i = 1; i < d; i++)
		cplx(c[i], 0, 0);
	cplx(c[0], 1, 0);
	/* check for diagonal */
	for (i = 0; i < d; i++) {
		if (real(a[i*d+i]) == 0 && imag(a[i*d+i]) == 0) {
			fprintf(stderr, "fcgaussseidel: matrix diagonal zero a[%u][%u]=%g%+gi\n", i, i, real(a[i*d+i]), imag(a[i*d+i]));
			return -1;
		}
		cinv(adiag[i], a[i*d+i]);
	}
	/* iteration */
	for (i = 0; i < iter; i++) {
		for (j = 0; j < d; j++) {
			s = b[j];
			for (k = 0; k < j; k++)
				cmsub(s, a[j*d+k], c[k]);
			for (k = j+1; k < d; k++)
				cmsub(s, a[j*d+k], c[k]);
			cmul(c[j], s, adiag[j]);
		}
	}
	return 0;
}
