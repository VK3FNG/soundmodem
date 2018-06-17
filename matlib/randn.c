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
#include <stdlib.h>

/*
 * Approximation of a normally distributed random number generator
 * with unit variance
 *
 * division factor: randommax * sqrt(nrand / 12)
 * with nrand = 16 and randommax = 0x1000
 */

float randn(void)
{
        int sum = 0, i;

        for (i = 0; i < 16; i++)
                sum += random() & 0xfff;
        return (sum - 0x8000) * (1.0 / 4729.7);
}

