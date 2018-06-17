#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "modem.h"
#include "modemconfig.h"
#include "filter.h"

#include <stdio.h>
#include <string.h>

#define	FilterLen	(AliasFilterLen * NumFilters)

/*
 * Sinc done properly.
 */
static inline double sinc(double x)
{
	return (fabs(x) < 1e-10) ? 1.0 : (sin(x) / x);
}

/*
 * Don't ask...
 */
static inline double cosc(double x)
{
	return (fabs(x) < 1e-10) ? 0.0 : (1 - cos(x)) / x;
}

/*
 * Hamming window function.
 */
static inline double hamming(double x)
{
	return 0.54 - 0.46 * cos(2 * M_PI * x);
}

void init_filter(struct filter *f, float rate, float f1, float f2)
{
	float t, h, x, sum, max;
	int i, j;

	f1 /= NumFilters;
	f2 /= NumFilters;

	f->phase = 0.0;
	f->phaseinc = 1.0 / rate;

	for (i = 0; i < FilterLen; i++) {
		t = i - (FilterLen - 1.0) / 2.0;
		h = i * (1.0 / (FilterLen - 1.0));

		x = (2 * f2 * sinc(2 * M_PI * f2 * t) -
		     2 * f1 * sinc(2 * M_PI * f1 * t)) * hamming(h);
		f->filtI[i % NumFilters][i / NumFilters] = x;

		x = (2 * f2 * cosc(2 * M_PI * f2 * t) -
		     2 * f1 * cosc(2 * M_PI * f1 * t)) * hamming(h);
		f->filtQ[i % NumFilters][i / NumFilters] = -x;
	}

	max = 0.0;
	for (i = 0; i < NumFilters; i++) {
		sum = 0.0;
		for (j = 0; j < AliasFilterLen; j++)
			sum += fabs(f->filtI[i][j]);

		max = (sum > max) ? sum : max;

		sum = 0.0;
		for (j = 0; j < AliasFilterLen; j++)
			sum += fabs(f->filtQ[i][j]);

		max = (sum > max) ? sum : max;
	}

	for (i = 0; i < NumFilters; i++) {
		for (j = 0; j < AliasFilterLen; j++) {
			f->filtI[i][j] /= max;
			f->filtQ[i][j] /= max;
		}
	}
}

int filter(struct filter *f, complex *in, complex *out)
{
	float *iptr = f->bufI;
	float *qptr = f->bufQ;
	int i, o, idx;

	memmove(iptr, iptr + SymbolLen, AliasFilterLen * sizeof(float));
	memmove(qptr, qptr + SymbolLen, AliasFilterLen * sizeof(float));

	iptr += AliasFilterLen;
	qptr += AliasFilterLen;
	for (i = 0; i < SymbolLen; i++) {
		*iptr++ = in[i].re;
		*qptr++ = in[i].im;
	}

	i = 0;
	o = 0;
	iptr = f->bufI;
	qptr = f->bufQ;
	while (i < SymbolLen) {
		if (f->phase >= 1.0) {
			f->phase -= 1.0;
			i++;
		} else {
			idx = NumFilters - 1 - (int) (f->phase * NumFilters);
			out[o].re = mac(iptr + i, f->filtI[idx], AliasFilterLen);
			out[o].im = mac(qptr + i, f->filtQ[idx], AliasFilterLen);
			f->phase += f->phaseinc;
			o++;
		}
	}
	return o;
}

