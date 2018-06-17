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

#include <string.h>
#include "mat.h"

/* Transpose a matrix (a el C^{d1 x d2}, b el C^{d2 x d1}) */

void fchermtranspose(cplxfloat_t *b, const cplxfloat_t *a, unsigned int d1, unsigned int d2)
{
	const cplxfloat_t *c = a;
	unsigned int ci, bi, i, j;

	if (b == a) {
		c = alloca(d1 * d2 * sizeof(c[0]));
		memcpy((void *)c, a, d1 * d2 * sizeof(c[0]));
	}
	for (i = 0; i < d1; i++)
		for (j = 0; j < d2; j++) {
			ci = i*d2+j;
			bi = j*d1+i;
			conj(b[bi], c[ci]);
		}
}
