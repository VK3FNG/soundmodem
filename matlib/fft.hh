/*****************************************************************************/

/*
 *      fft.hh  --  Simple RIF FFT function.
 *
 *      Copyright (C) 2003
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

/*****************************************************************************/

#ifndef _FFT_HH
#define _FFT_HH

/* --------------------------------------------------------------------- */

/*
 * This fft routine is from ~gabriel/src/filters/fft/fft.c;
 * I am unsure of the original source.  The file contains no
 * copyright notice or description.
 * The declaration is changed to the prototype form but the
 * function body is unchanged.  (J. T. Buck)
 */

/*
 * Replace data by its discrete Fourier transform, if isign is
 * input as 1, or by its inverse discrete Fourier transform, if
 * "isign" is input as -1.  "data'"is a complex array of length "nn".
 * "nn" MUST be an integer power of 2 (this is not checked for!?)
 */

template<typename T> static void fft_rif(complex<T> *data, unsigned int nn, int isign)
{
        for (unsigned int i = 0, j = 0; i < nn; i++) {
                if (j > i) {
			complex<T> temp = data[j];
			data[j] = data[i];
			data[i] = temp;
                }
                unsigned int m = nn >> 1;
                while (m > 0 && (int)j >= (int)m) {
                        j -= m;
                        m >>= 1;
                }
                j += m;
        }
        unsigned int mmax = 1;
	T theta = -6.28318530717959 * 0.5;
	if (isign < 0)
		theta = -theta;
	T sintheta = sin(theta);
        while (nn > mmax) {
		T oldsintheta = sintheta;
		theta *= 0.5;
		sintheta = sin(theta);
		complex<T> wp(-2.0 * sintheta * sintheta, oldsintheta); /* -2.0 * sin(0.5*theta)^2 = cos(theta)-1 */
		complex<T> w(1,0);
                for (unsigned int m = 0; m < mmax; m++) {
                        for (unsigned int i = m; i < nn; i += 2*mmax) {
                                unsigned int j = i + mmax;
				complex<T> temp = w * data[j];
				data[j] = data[i] - temp;
				data[i] += temp;
                        }
			w += w * wp;
                }
                mmax <<= 1;
        }
}

/* ---------------------------------------------------------------------- */
#endif /* _FFT_HH */
