#ifndef _FILTER_H
#define _FILTER_H

#include "complex.h"

/* ---------------------------------------------------------------------- */

#ifdef __i386__
#include "filter-i386.h"
#endif				/* __i386__ */

/* ---------------------------------------------------------------------- */

#ifndef __HAVE_ARCH_MAC
static inline float mac(const float *a, const float *b, unsigned int size)
{
	float sum = 0;
	unsigned int i;

	for (i = 0; i < size; i++)
		sum += (*a++) * (*b++);
	return sum;
}
#endif				/* __HAVE_ARCH_MAC */

/* ---------------------------------------------------------------------- */

struct filter {
	float filtI[NumFilters][AliasFilterLen];
	float filtQ[NumFilters][AliasFilterLen];
	float bufI[AliasFilterLen + SymbolLen];
	float bufQ[AliasFilterLen + SymbolLen];
	float phase;
	float phaseinc;
};

extern void init_filter(struct filter *, float, float, float);
extern int filter(struct filter *, complex *, complex *);

/* ---------------------------------------------------------------------- */
#endif				/* _FILTER_H */
