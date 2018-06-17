#ifndef _COMPLEX_H
#define _COMPLEX_H

#include <math.h>

typedef struct {
	float re, im;
#ifdef __ia64__
	int dummy;
#endif
} complex;

/*
 * Complex multiplication.
 */
extern __inline__ complex cmul(complex x, complex y)
{
	complex z;

	z.re = x.re * y.re - x.im * y.im;
	z.im = x.re * y.im + x.im * y.re;

	return z;
}

/*
 * Complex ... yeah, what??? Returns a complex number that has the
 * properties: |z| = |x| * |y|  and  arg(z) = arg(y) - arg(x)
 */
extern __inline__ complex ccor(complex x, complex y)
{
	complex z;

	z.re = x.re * y.re + x.im * y.im;
	z.im = x.re * y.im - x.im * y.re;

	return z;
}

/*
 * Real part of the complex ???
 */
extern __inline__ float ccorI(complex x, complex y)
{
	return x.re * y.re + x.im * y.im;
}

/*
 * Imaginary part of the complex ???
 */
extern __inline__ float ccorQ(complex x, complex y)
{
	return x.re * y.im - x.im * y.re;
}

/*
 * Modulo (absolute value) of a complex number.
 */
extern __inline__ float cmod(complex x)
{
	return sqrt(x.re * x.re + x.im * x.im);
}

/*
 * Square of the absolute value (power).
 */
extern __inline__ float cpwr(complex x)
{
	return (x.re * x.re + x.im * x.im);
}

/*
 * Argument of a complex number.
 */
extern __inline__ float carg(complex x)
{
	return atan2(x.im, x.re);
}

#endif
