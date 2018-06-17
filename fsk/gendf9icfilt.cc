/*****************************************************************************/

/*
 *      gendf9icfilt.cc  --  Compute DF9IC Hardware Modem Filter Curves.
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

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <complex>
#include <iostream>
#include <sstream>

using namespace std;

#include "fft.hh"
#include "mat.hh"
#include "raisedcosine.h"

#include "getopt.h"

/* --------------------------------------------------------------------- */

template<typename T> complex<T> rxfilter(T freq)
{
	complex<T> g1(T(1)/100e3,0);
	complex<T> g2(T(1)/82e3,0);
	complex<T> g3(T(1)/39e3,0);
	complex<T> g4(T(1)/27e3,0);
	freq *= 2 * M_PI * 9600;
	complex<T> y1(0,freq*470e-9);
	complex<T> y2(0,freq*1e-9);
	complex<T> y3(0,freq*220e-12);
	complex<T> y4(0,freq*1e-9);

	return y1 * g2 * g3 * g4 / 
		(((g3 + y3) * g2 * (y1 + g1) + y3 * (y2 + g3) * (y1 + g1 + g2)) * (g4 + y4));
}

template<typename T> complex<T> txfilter(T freq)
{
	complex<T> g1(T(1)/100,0);
	complex<T> g2(T(1)/10e3,0);
	complex<T> g3(T(1)/100e3,0);
	complex<T> g4(T(1)/100e3,0);
	complex<T> g5(T(1)/56e3,0);
	complex<T> g6(T(1)/8.2e3,0);
	complex<T> g7(T(1)/12e3,0);
	freq *= 2 * M_PI * 9600;
	complex<T> y1(0,freq*3.3e-9);
	complex<T> y2(0,freq*470e-12);
	complex<T> y3(0,freq*100e-12);
	complex<T> y4(0,freq*1e-9);

	return g1 * g4 * g5 * g6 /
		(((g1 + g2 + g3 + y1) * (g5 + y3) * g4 + 
		  (g1 + g2 + g3 + y1 + g4) * y3 * (y2 + g5)) * (y4 + g7));
}

/* ---------------------------------------------------------------------- */

static void printtransferfunc(ostream& os, unsigned int nr, double over)
{
	over /= nr;
       	os << "# name: rxf\n"
		"# type: complex matrix\n"
		"# rows: " << nr << "\n"
		"# columns: 1\n";
	for (unsigned int i = 0; i < nr; i++) {
		complex<double> tf = rxfilter(i * over);
		os << "(" << tf.real() << "," << tf.imag() << ")\n";
	}
       	os << "# name: txf\n"
		"# type: complex matrix\n"
		"# rows: " << nr << "\n"
		"# columns: 1\n";
	for (unsigned int i = 0; i < nr; i++) {
		complex<double> tf = txfilter(i * over);
		os << "(" << tf.real() << "," << tf.imag() << ")\n";
	}
	os.flush();
}

/* ---------------------------------------------------------------------- */

template<typename T> static void matprintf(ostream& os, const char *name,
					   unsigned int size1, unsigned int stride1,
					   unsigned int size2, unsigned int stride2, const T *m)
{
	os << "# name: " << name << "\n# type: matrix\n# rows: " << size1 << "\n# columns: " << size2 << "\n";
	for (unsigned int i = 0; i < size1; i++) {
		for (unsigned int j = 0; j < size2; j++)
			os << " " << m[i*stride1 + j*stride2];
		os << "\n";
	}
}

/* ---------------------------------------------------------------------- */

static double comptx(ostream& os, double *filt, const double *pulse, unsigned int filtlen, unsigned int pulselen, 
		     unsigned int over, unsigned int odiv, double shift, unsigned int oct)
{
        unsigned int rlen = 2 * filtlen + pulselen;
        double C[rlen * filtlen];
        double CT[rlen * filtlen];
        double CTC[filtlen * filtlen];
        double CTr[filtlen];
        double Cf[rlen];
        double r[rlen];
        double e, e1;
        double rcosalpha = 3.0 / 8;
        int pidx;
        unsigned int i, j;

	for (i = 0; i < rlen; i++) {
                pidx = i - 2 * filtlen + 1;
                if (pidx >= (signed int)pulselen) {
                        rlen = i;
                        break;
                }
                for (j = 0; j < filtlen; j++, pidx += odiv) {
                        if (pidx < 0 || pidx >= (int)pulselen)
                                C[i*filtlen+j] = 0;
                        else
                                C[i*filtlen+j] = pulse[pidx];
                }
        }
        for (i = 0; i < rlen; i++)
                r[i] = raised_cosine_time((i - 0.5 * (rlen-1)) / (double)over + shift, rcosalpha);
	if (oct) {
		matprintf(os, "C", rlen, filtlen, filtlen, 1, C);
		matprintf(os, "r", rlen, 1, 1, 1, r);
	}
        mtranspose(CT, C, rlen, filtlen);
        mmul(CTC, CT, C, filtlen, rlen, filtlen);
        mmul(CTr, CT, r, filtlen, rlen, 1);
        mchol(CTC, CTr, filt, filtlen);
	if (oct)
		matprintf(os, "filt", filtlen, 1, 1, 1, filt);
        mmul(Cf, C, filt, rlen, filtlen, 1);
        for (i = 0, e = 0; i < rlen; i++) {
                e1 = Cf[i] - r[i];
                e += e1 * e1;
        }
	return e;
}

static void printfcoeff(ostream& os, unsigned int fftsz, unsigned int over, unsigned int oct)
{
	if (oct)
		printtransferfunc(os, fftsz, over);
	else
		os << "/* this file is automatically generated, do not edit!! */\n\n";
	complex<double> fftb[fftsz];
	double rpulse[fftsz], tpulse[fftsz];
	double dover = double(over) / fftsz;
	double sum;

	/* compute rx pulse */
	for (unsigned int i = 1; i < fftsz/2; i++)
		fftb[i] = rxfilter(i * dover);
	fftb[0] = complex<double>(fftb[1].real(), 0);
	fftb[fftsz/2] = 0;
	for (unsigned int i = 1; i < fftsz/2; i++)
		fftb[fftsz-i] = conj(fftb[i]);
	if (oct) {
		os << "# name: rxs\n"
			"# type: complex matrix\n"
			"# rows: " << fftsz << "\n"
			"# columns: 1\n";
		for (unsigned int i = 0; i < fftsz; i++)
			os << "(" << fftb[i].real() << "," << fftb[i].imag() << ")\n";
	}
	fft_rif(fftb, fftsz, -1);
	sum = 0;
	for (unsigned int i = fftsz/2; i < fftsz; i++)
		sum -= fftb[i].real();
	sum *= double(2) / fftsz;
	for (unsigned int i = 0; i < fftsz; i++)
		rpulse[i] = dover * (fftb[i].real() + sum);
	if (oct) {
		os << "# name: rx\n"
			"# type: matrix\n"
			"# rows: " << fftsz << "\n"
			"# columns: 1\n";
		for (unsigned int i = 0; i < fftsz; i++)
			os << rpulse[i] << "\n";
	} else {
		os << "#define FSKIC_RXOVER " << over << "\n";
		os << "\nstatic float fskic_rxpulse[" << 4 * over << "] = {";
		for (unsigned int i = 0;;) {
			if (!(i & 3))
				os << "\n\t";
			os << " " << rpulse[i];
			i++;
			if (i >= 4 * over)
				break;
			os << ",";
		}
		os << "\n};\n\n";
	}
	/* compute tx pulse */
	for (unsigned int i = 1; i < fftsz/2; i++)
		fftb[i] = txfilter(i * dover);
	fftb[0] = complex<double>(fftb[1].real(), 0);
	fftb[fftsz/2] = 0;
	for (unsigned int i = 1; i < fftsz/2; i++)
		fftb[fftsz-i] = conj(fftb[i]);
	if (oct) {
		os << "# name: txs\n"
			"# type: complex matrix\n"
			"# rows: " << fftsz << "\n"
			"# columns: 1\n";
		for (unsigned int i = 0; i < fftsz; i++)
			os << "(" << fftb[i].real() << "," << fftb[i].imag() << ")\n";
	}
	fft_rif(fftb, fftsz, -1);
	sum = 0;
	for (unsigned int i = fftsz/2; i < fftsz; i++)
		sum -= fftb[i].real();
	sum *= double(2) / fftsz;
	for (unsigned int i = 0; i < fftsz; i++)
		tpulse[i] = dover * (fftb[i].real() + sum);
	if (oct) {
		os << "# name: tx\n"
			"# type: matrix\n"
			"# rows: " << fftsz << "\n"
			"# columns: 1\n";
		for (unsigned int i = 0; i < fftsz; i++)
			os << tpulse[i] << "\n";
	}
	/* compute tx compensation filter */
	double filt[8 * over];
	double shift = 0;
	ostringstream oss;
	double err = comptx(oss, filt, rpulse, sizeof(filt) / sizeof(filt[0]), 4 * over, over, 2, shift, oct);
	double maxf = 0;
	for (unsigned int i = 0; i < sizeof(filt) / sizeof(filt[0]); i++)
		if (filt[i] > maxf) {
			maxf = filt[i];
			shift = i;
		}
	shift -= sizeof(filt) / sizeof(filt[0]) / double(2);
	shift *= double(2) / over;
	shift = -shift;
	err = comptx(os, filt, rpulse, sizeof(filt) / sizeof(filt[0]), 4 * over, over, 2, shift, oct);
	if (oct) {
		os << "# name: err\n"
		   << "# type: scalar\n"
		   << err << "\n";
	} else {
		os << "#define FSKIC_TXOVER " << over / 2 << "\n";
		os << "\nstatic float fskic_txpulse[" << sizeof(filt) / sizeof(filt[0]) - 2 * over << "] = {";
		for (unsigned int i = over;;) {
			if (!(i & 3))
				os << "\n\t";
			os << " " << filt[i];
			i++;
			if (i >= sizeof(filt) / sizeof(filt[0]) - 2 * over)
				break;
			os << ",";
		}
		os << "\n};\n\n";
	}
}

/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
        static const struct option long_options[] = {
		{ 0, 0, 0, 0 }
        };
        int c, err = 0;
	unsigned int oct = 0;

        while ((c = getopt_long(argc, argv, "o", long_options, NULL)) != EOF) {
                switch (c) {
		case 'o':
			oct = 1;
			break;

                default:
                        err++;
                        break;
                }
        }
        if (err) {
                cerr << "usage: [-o]\n";
                exit(1);
        }
	printfcoeff(cout, 2048, 16, oct);
        return 0;
}
