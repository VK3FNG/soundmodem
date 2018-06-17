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

/*
 * (C) 1999 Institut für Elektronik, ETH Zürich
 * Author: Thomas Sailer, <sailer@ife.ee.ethz.ch>
 * KTI Projekt
 */

#ifndef _MAT_H
#define _MAT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

/*
 * basic complex manipulations
 */

typedef struct {
	float re;
	float im;
} cplxfloat_t;

typedef struct {
	double re;
	double im;
} cplxdouble_t;

#define real(r)       ((r).re)
#define imag(r)       ((r).im)
#define conj(r,a)     do { (r).re = (a).re; (r).im = -(a).im; } while (0)
#define cplx(v,r,i)   do { (v).re = (r); (v).im = (i); } while (0)

#define csub(r,a,b)                                          \
	do {                                                 \
		double xr,xi,yr,yi;                          \
		xr = (a).re; xi = (a).im;                    \
                yr = (b).re; yi = (b).im;                    \
		(r).re = xr - yr;                            \
		(r).im = xi - yi;                            \
	} while (0)

#define cadd(r,a,b)                                          \
	do {                                                 \
		double xr,xi,yr,yi;                          \
		xr = (a).re; xi = (a).im;                    \
                yr = (b).re; yi = (b).im;                    \
		(r).re = xr + yr;                            \
		(r).im = xi + yi;                            \
	} while (0)

#define cmul(r,a,b)                                          \
	do {                                                 \
		double xr,xi,yr,yi;                          \
		xr = (a).re; xi = (a).im;                    \
                yr = (b).re; yi = (b).im;                    \
		(r).re = xr * yr - xi * yi;                  \
		(r).im = xr * yi + xi * yr;                  \
	} while (0)

#define cinv(r,a)                                            \
	do {                                                 \
		double xr,xi,mag;                            \
		xr = (a).re; xi = (a).im;                    \
		mag = 1 / (xr * xr + xi * xi);               \
		(r).re = xr * mag;                           \
		(r).im = -xi * mag;                          \
	} while (0)

#define cmac(r,a,b)                                          \
	do {                                                 \
		double xr,xi,yr,yi;                          \
		xr = (a).re; xi = (a).im;                    \
                yr = (b).re; yi = (b).im;                    \
		(r).re += xr * yr - xi * yi;                 \
		(r).im += xr * yi + xi * yr;                 \
	} while (0)

#define cmsub(r,a,b)                                         \
	do {                                                 \
		double xr,xi,yr,yi;                          \
		xr = (a).re; xi = (a).im;                    \
                yr = (b).re; yi = (b).im;                    \
		(r).re -= xr * yr - xi * yi;                 \
		(r).im -= xr * yi + xi * yr;                 \
	} while (0)

#define cfma(r,a,b,c)                                        \
	do {                                                 \
		double xr,xi,yr,yi,zr,zi;                    \
		xr = (a).re; xi = (a).im;                    \
                yr = (b).re; yi = (b).im;                    \
                zr = (c).re; zi = (c).im;                    \
		(r).re = xr * yr - xi * yi + zr;             \
		(r).im = xr * yi + xi * yr + zi;             \
	} while (0)

#define cmuls(r,a,b)                                         \
	do {                                                 \
		double xr,xi,bb;                             \
		xr = (a).re; xi = (a).im; bb = (b);          \
		(r).re = xr * bb;                            \
		(r).im = xi * bb;                            \
	} while (0)

#define cmacs(r,a,b)                                         \
	do {                                                 \
		double xr,xi,bb;                             \
		xr = (a).re; xi = (a).im; bb = (b);          \
		(r).re += xr * bb;                           \
		(r).im += xi * bb;                           \
	} while (0)

#define cmsubs(r,a,b)                                        \
	do {                                                 \
		double xr,xi,bb;                             \
		xr = (a).re; xi = (a).im; bb = (b);          \
		(r).re -= xr * bb;                           \
		(r).im -= xi * bb;                           \
	} while (0)

/*
 * Gaussian distributed random variables
 */
extern float randn(void);

/*
 * Cholesky solver
 */

extern int frcholfactor(const float *a, float *g, unsigned int d);
extern void frcholapply(const float *g, const float *b, float *c, unsigned int d);
extern int frchol(const float *a, const float *b, float *c, unsigned int d);
extern int fccholfactor(const cplxfloat_t *a, cplxfloat_t *g, unsigned int d);
extern void fccholapply(const cplxfloat_t *g, const cplxfloat_t *b, cplxfloat_t *c, unsigned int d);
extern int fcchol(const cplxfloat_t *a, const cplxfloat_t *b, cplxfloat_t *c, unsigned int d);
extern int drcholfactor(const double *a, double *g, unsigned int d);
extern void drcholapply(const double *g, const double *b, double *c, unsigned int d);
extern int drchol(const double *a, const double *b, double *c, unsigned int d);
extern int dccholfactor(const cplxdouble_t *a, cplxdouble_t *g, unsigned int d);
extern void dccholapply(const cplxdouble_t *g, const cplxdouble_t *b, cplxdouble_t *c, unsigned int d);
extern int dcchol(const cplxdouble_t *a, const cplxdouble_t *b, cplxdouble_t *c, unsigned int d);

/*
 * Gauss-Seidel iterative solver
 */
extern int frgaussseidel(const float *a, const float *b, float *c, unsigned int d, unsigned int iter);
extern int fcgaussseidel(const cplxfloat_t *a, const cplxfloat_t *b, cplxfloat_t *c, unsigned int d, unsigned int iter);

/*
 * Transpose etc.
 */

extern void frtranspose(float *b, const float *a, unsigned int d1, unsigned int d2);
extern void fradd(float *c, const float *a, const float *b, unsigned int d1, unsigned int d2);
extern void frsub(float *c, const float *a, const float *b, unsigned int d1, unsigned int d2);
extern void frmul(float *c, const float *a, const float *b, unsigned int d1, unsigned int d2, unsigned int d3);
extern float frdet(const float *c, unsigned int d);

extern void frlufact(float *u, unsigned int *p, const float *a, unsigned int d);
extern void frlusolve(float *x, const float *b, const float *u, const unsigned int *p, unsigned int d);
extern void frinv(float *ainv, const float *a, unsigned d);

extern void fctranspose(cplxfloat_t *b, const cplxfloat_t *a, unsigned int d1, unsigned int d2);
extern void fchermtranspose(cplxfloat_t *b, const cplxfloat_t *a, unsigned int d1, unsigned int d2);
extern void fcconj(cplxfloat_t *c, const cplxfloat_t *a, unsigned int d1, unsigned int d2);
extern void fcadd(cplxfloat_t *c, const cplxfloat_t *a, const cplxfloat_t *b, unsigned int d1, unsigned int d2);
extern void fcsub(cplxfloat_t *c, const cplxfloat_t *a, const cplxfloat_t *b, unsigned int d1, unsigned int d2);
extern void fcmul(cplxfloat_t *c, const cplxfloat_t *a, const cplxfloat_t *b, unsigned int d1, unsigned int d2, unsigned int d3);
extern cplxfloat_t fcdet(const cplxfloat_t *c, unsigned int d);

extern void fclufact(cplxfloat_t *u, unsigned int *p, const cplxfloat_t *a, unsigned int d);
extern void fclusolve(cplxfloat_t *x, const cplxfloat_t *b, const cplxfloat_t *u, const unsigned int *p, unsigned int d);
extern void fcinv(cplxfloat_t *ainv, const cplxfloat_t *a, unsigned d);


#endif /* _MAT_H */
