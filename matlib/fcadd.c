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

#include "mat.h"

void fcadd(cplxfloat_t *c, const cplxfloat_t *a, const cplxfloat_t *b, unsigned int d1, unsigned int d2)
{
	unsigned int i, j = d1 * d2;

	for (i = 0; i < j; i++)
		cadd(c[i], a[i], b[i]);
}

