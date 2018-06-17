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

/* c el R^{d1 x d3}, a el R^{d1 x d2}, b el R^{d2 x d3} */

void frmul(float *c, const float *a, const float *b, unsigned int d1, unsigned int d2, unsigned int d3)
{
	float *r = c, s;
	unsigned int i, j, k;
	
	if (c == a || c == b)
		r = alloca(d1 * d3 * sizeof(r[0]));
	for (i = 0; i < d1; i++)
		for (k = 0; k < d3; k++) {
			for (s = 0, j = 0; j < d2; j++)
				s += a[i*d2+j] * b[j*d3+k];
			r[i*d3+k] = s;
		}
	if (r != c)
		memcpy(c, r, d1 * d3 * sizeof(c[0]));
}
