/*****************************************************************************/

/*
 *      gentbl.c  -- Soundmodem table generator.
 *
 *      Copyright (C) 1996-2001
 *        Thomas Sailer (t.sailer@alumni.ethz.ch)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------- */

static const char *progname;

/* -------------------------------------------------------------------- */

static void gen_costab(unsigned size)
{
	FILE *f;
        int i;

	if (!(f = fopen("costab.c", "w"))) {
		fprintf(stderr, "cannot open file costab.c\n");
		exit(1);
	}
	fprintf(f, "#include \"costab.h\"\n\n/*\n * cosine table\n */\n\n"
		"const int16_t afsk_costab[%d] = {", size);
        for (i = 0; i < size; i++) {
                if (!(i & 7))
                        fprintf(f, "\n\t");
                fprintf(f, "%6d", (int)(32767.0*cos(i*(2.0*M_PI)/size)));
                if (i != (size-1))
                        fprintf(f, ", ");
        }
        fprintf(f, "\n};\n\n");
}

/* -------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	progname = argv[0];
	gen_costab(512);
	exit(0);
}

/* -------------------------------------------------------------------- */
