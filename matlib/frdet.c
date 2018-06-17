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

float frdet(const float *c, unsigned int d)
{
	float *c2;
	unsigned int i, j, k, l;
	float det = 0, dr;

	if (!d)
		return 0;
	if (d == 1)
		return c[0];
	if (d == 2)
		return c[0] * c[3] - c[1] * c[2];
	c2 = alloca(sizeof(float)*(d-1)*(d-1));
	for (i = 0; i < d; i++) {
		for (j = k = 0; j < d; j++) {
			if (j == i)
				continue;
			for (l = 0; l < d-1; l++)
				c2[l*(d-1)+k] = c[(l+1)*d+j];
			k++;
		}
		dr = frdet(c2, d-1);
		if (i & 1)
			det -= dr * c[i];
		else
			det += dr * c[i];
	}
	return det;
}

