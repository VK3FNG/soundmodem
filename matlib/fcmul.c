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
#include <config.h>
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

/* c el C^{d1 x d3}, a el C^{d1 x d2}, b el C^{d2 x d3} */

void fcmul(cplxfloat_t *c, const cplxfloat_t *a, const cplxfloat_t *b, unsigned int d1, unsigned int d2, unsigned int d3)
{
	cplxfloat_t *r = c, s;
	unsigned int i, j, k;
	
	if (c == a || c == b)
		r = alloca(d1 * d3 * sizeof(r[0]));
	for (i = 0; i < d1; i++)
		for (k = 0; k < d3; k++) {
			cplx(s, 0, 0);
			for (j = 0; j < d2; j++)
				cmac(s, a[i*d2+j], b[j*d3+k]);
			r[i*d3+k] = s;
		}
	if (r != c)
		memcpy(c, r, d1 * d3 * sizeof(c[0]));
}
