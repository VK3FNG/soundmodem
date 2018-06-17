/*
 * Matrix operations library
 *
 * Copyright (C) 1999-2000, 2003
 *   Thomas Sailer, <t.sailer@alumni.ethz.ch>
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

#ifndef _MAT_HH
#define _MAT_HH

#ifdef HAVE_CONFIG_H
#include <config.h>
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

#include <exception>
#include <string>
#include <sstream>

class matrix_exception : public exception
{
public:
	matrix_exception(const string& e) throw() : err(e) {}
	virtual ~matrix_exception() throw() {}
	virtual const char* what() const throw() { return err.c_str(); }
private:
        const string err;
};


/*
 * Simple matrix operations
 */

template<typename T> void madd(T *c, const T *a, const T *b, unsigned int d1, unsigned int d2)
{
        unsigned int i, j = d1 * d2;
        for (i = 0; i < j; i++)
                c[i] = a[i] + b[i];
}

template<typename T> void msub(T *c, const T *a, const T *b, unsigned int d1, unsigned int d2)
{
        unsigned int i, j = d1 * d2;
        for (i = 0; i < j; i++)
                c[i] = a[i] - b[i];
}

/* c el R^{d1 x d3}, a el R^{d1 x d2}, b el R^{d2 x d3} */
template<typename T> void mmul(T *c, const T *a, const T *b, unsigned int d1, unsigned int d2, unsigned int d3)
{
        T *r = c, s;
        unsigned int i, j, k;
        if (c == a || c == b)
                r = (T *)alloca(d1 * d3 * sizeof(r[0]));
        for (i = 0; i < d1; i++)
                for (k = 0; k < d3; k++) {
                        for (s = 0, j = 0; j < d2; j++)
                                s += a[i*d2+j] * b[j*d3+k];
                        r[i*d3+k] = s;
                }
        if (r != c)
                memcpy(c, r, d1 * d3 * sizeof(c[0]));
}

template<typename T> void mdet(const T *c, unsigned int d)
{
        T *c2;
        unsigned int i, j, k, l;
        T det = 0, dr;

        if (!d)
                return 0;
        if (d == 1)
                return c[0];
        if (d == 2)
                return c[0] * c[3] - c[1] * c[2];
        c2 = alloca(sizeof(T)*(d-1)*(d-1));
        for (i = 0; i < d; i++) {
                for (j = k = 0; j < d; j++) {
                        if (j == i)
                                continue;
                        for (l = 0; l < d-1; l++)
                                c2[l*(d-1)+k] = c[(l+1)*d+j];
                        k++;
                }
                dr = mdet(c2, d-1);
                if (i & 1)
                        det -= dr * c[i];
                else
                        det += dr * c[i];
        }
        return det;
}

/* Transpose a matrix (a el C^{d1 x d2}, b el C^{d2 x d1}) */
template<typename T> void mtranspose(T *b, const T *a, unsigned int d1, unsigned int d2)
{
        const T *c = a;
        unsigned int ci, bi, i, j;

        if (b == a) {
                void *cc = alloca(d1 * d2 * sizeof(c[0]));
                memcpy(cc, a, d1 * d2 * sizeof(c[0]));
		c = (const T *)cc;
        }
        for (i = 0; i < d1; i++)
                for (j = 0; j < d2; j++) {
                        ci = i*d2+j;
                        bi = j*d1+i;
                        b[bi] = c[ci];
                }
}

/* Transpose a matrix (a el C^{d1 x d2}, b el C^{d2 x d1}) */
template<typename T> void mhermtranspose(complex<T> *b, const complex<T> *a, unsigned int d1, unsigned int d2)
{
        const complex<T> *c = a;
        unsigned int ci, bi, i, j;

        if (b == a) {
                void *cc = alloca(d1 * d2 * sizeof(c[0]));
                memcpy(cc, a, d1 * d2 * sizeof(c[0]));
		c = (const complex<T> *)cc;
        }
        for (i = 0; i < d1; i++)
                for (j = 0; j < d2; j++) {
                        ci = i*d2+j;
                        bi = j*d1+i;
                        b[bi] = conj(c[ci]);
                }
}

template<typename T> void mconj(complex<T> *c, const complex<T> *a, unsigned int d1, unsigned int d2)
{
        unsigned int i, j = d1 * d2;
        for (i = 0; i < j; i++)
                c[i] = conj(a[i]);
}

/*
 * complex cholesky factorization
 */

/*
 * A el C^{d x d}
 * This routine calculates G*G^H = A, where G is lower triangular, and then uses this to solve
 * A*c=b for c
 * G*G^H*c=b
 * G*t=b
 * G^H*c=t
 */

template<typename T> inline T complex_pwr(complex<T> c)
{
        return real(c) * real(c) + imag(c) * imag(c);
}

template<typename T> void mcholfactor(const complex<T> *a, complex<T> *g, unsigned int d)
{
	unsigned int i, j, k;
	complex<T> sc;
	T s;

	memset(g, 0, d*d*sizeof(g[0]));
	for (i = 0; i < d; i++) {
		s = real(a[i*d+i]);
		for (j = 0; j < i; j++)
			s -= complex_pwr(g[i*d+j]);
		if (s <= 0 || imag(a[i*d+i]) != 0) {
			ostringstream os;
			os << "mcholfactor: matrix not positive definite a[" << i << "][" << i << "]=" << a[i*d+i] << " s=" << s << "\n";
			throw matrix_exception(os.str());
		}
		s = T(1) / sqrt(s);
		g[i*d+i] = s;
		for (j = i+1; j < d; j++) {
			sc = a[j*d+i];
			for (k = 0; k < i; k++)
				sc -= g[j*d+k] * conj(g[i*d+k]);
			g[j*d+i] = sc * s;
		}
	}
}

template<typename T> void mcholapply(const complex<T> *g, const complex<T> *b, complex<T> *c, unsigned int d)
{
	complex<T> *t, s, s2;
	unsigned int i, j;

	t = alloca(d*sizeof(t[0]));
	for (i = 0; i < d; i++) {
		s = b[i];
		for (j = 0; j < i; j++)
			s -= g[i*d+j] * t[j];
		/* g's diagonal is real, therefore we have a division by a real */
		t[i] =  s * real(g[i*d+i]);
	}
	for (i = d; i > 0; i--) {
		s = t[i-1];
		for (j = i; j < d; j++)
			s -= conj(g[j*d+(i-1)]) * c[j];
		/* g's diagonal is real, therefore we have a division by a real */
		c[i-1] = s * real(g[(i-1)*d+(i-1)]);
	}
}

/*
 * real cholesky factorization
 */

/*
 * A el R^{d x d}
 * This routine calculates G*G^T = A, where G is lower triangular, and then uses this to solve
 * A*c=b for c
 * G*G^T*c=b
 * G*t=b
 * G^T*c=t
 */

template<typename T> void mcholfactor(const T *a, T *g, unsigned int d)
{
	unsigned int i, j, k;
	T s, s1;
	
	memset(g, 0, d*d*sizeof(g[0]));
	for (i = 0; i < d; i++) {
		s = a[i*d+i];
		for (j = 0; j < i; j++)
			s -= g[i*d+j] * g[i*d+j];
		if (s <= 0) {
			ostringstream os;
			os << "mcholfactor: matrix not positive definite a[" << i << "][" << i << "]=" << a[i*d+i] << " s=" << s << "\n";
			throw matrix_exception(os.str());
		}
		s = T(1) / sqrt(s);
		g[i*d+i] = s;
		for (j = i+1; j < d; j++) {
			s1 = a[j*d+i];
			for (k = 0; k < i; k++)
				s1 -= g[j*d+k] * g[i*d+k];
			g[j*d+i] = s * s1;
		}
	}
}

template<typename T> void mcholapply(const T *g, const T *b, T *c, unsigned int d)
{
	T *t;
	unsigned int i, j;
	float s1;

	t = (T *)alloca(d*sizeof(t[0]));
	for (i = 0; i < d; i++) {
		s1 = b[i];
		for (j = 0; j < i; j++)
			s1 -= g[i*d+j] * t[j];
		t[i] = s1 * g[i*d+i];
	}
	for (i = d; i > 0; i--) {
		s1 = t[i-1];
		for (j = i; j < d; j++)
			s1 -= g[j*d+(i-1)] * c[j];
		c[i-1] = s1 * g[(i-1)*d+(i-1)];
	}
}


template<typename T> void mchol(const T *a, const T *b, T *c, unsigned int d)
{
	T *g;
	g = (T *)alloca(d*d*sizeof(g[0]));
	mcholfactor(a, g, d);
	mcholapply(g, b, c, d);
}

/*
 * Complex Gaussian Elimination
 */

/*
 * Golub/van Loan, 3.1.3, p 112; PA=LU factorization with partial pivoting
 */

template<typename T> inline void swap(T& a, T& b)
{
	T c = a;
	a = b;
	b = c;
}

template<typename T> void mlufact(complex<T> *u, unsigned int *p, const complex<T> *a, unsigned int d)
{
	unsigned int i, j, k, mu;
	complex<T> fm;
	T f1, f2;

	if (u != a)
		memcpy(u, a, d*d*sizeof(u[0]));
	for (k = 0; k < d-1; k++) {
		/* search pivot index */
		for (f1 = 0, i = mu = k; i < d; i++) {
			f2 = complex_pwr(u[i*d+k]);
			if (f2 > f1) {
				f1 = f2;
				mu = i;
			}
		}
		/* exchange rows */
		p[k] = mu;
		for (i = k; i < d; i++)
			swap(u[k*d+i], u[mu*d+i]);
		fm = u[k*d+k];
		if (real(fm) != 0 || imag(fm) != 0) {
			fm = T(1) / fm;
			for (i = k+1; i < d; i++)
				u[i*d+k] *= fm;
			for (i = k+1; i < d; i++)
				for (j = k+1; j < d; j++)
					u[i*d+j] -= u[i*d+k] * u[k*d+j];
		}
	}
}

template<typename T> void mlusolve(complex<T> *x, const complex<T> *b, const complex<T> *u, const unsigned int *p, unsigned int d)
{
	complex<T> *y, s;
	unsigned int k, i;

	y = alloca(d * sizeof(y[0]));
	memcpy(y, b, d * sizeof(y[0]));
	for (k = 0; k < d-1; k++) {
		i = p[k];
		if (i != k)
			swap(y[k], y[i]);
		if (real(y[k]) == 0 && imag(y[k]) == 0)
			continue;
		for (i = k+1; i < d; i++)
			y[i] -= y[k] * u[i*d+k];
	}
	/* solve Ux=y */
	for (k = d; k > 0; k--) {
		s = y[k-1];
		for (i = k; i < d; i++)
			s -= u[(k-1)*d+i] * x[i];
		x[k-1] = s / u[(k-1)*d+(k-1)];
	}
}

/*
 * Real Gaussian Elimination
 */

/*
 * Golub/van Loan, 3.1.3, p 112; PA=LU factorization with partial pivoting
 */

template<typename T> void mlufact(T *u, unsigned int *p, const T *a, unsigned int d)
{
	unsigned int i, j, k, mu;
	T f1, f2;

	if (u != a)
		memcpy(u, a, d*d*sizeof(u[0]));
	for (k = 0; k < d-1; k++) {
		/* search pivot index */
		for (f1 = 0, i = mu = k; i < d; i++) {
			f2 = fabs(u[i*d+k]);
			if (f2 > f1) {
				f1 = f2;
				mu = i;
			}
		}
		/* exchange rows */
		p[k] = mu;
		for (i = k; i < d; i++)
			swap(u[k*d+i], u[mu*d+i]);
		f1 = u[k*d+k];
		if (f1 != 0) {
			f1 = 1 / f1;
			for (i = k+1; i < d; i++)
				u[i*d+k] *= f1;
			for (i = k+1; i < d; i++)
				for (j = k+1; j < d; j++)
					u[i*d+j] -= u[i*d+k] * u[k*d+j];
		}
	}
}

template<typename T> void mlusolve(T *x, const T *b, const T *u, const unsigned int *p, unsigned int d)
{
	T *y, s;
	unsigned int k, i;

	y = alloca(d * sizeof(y[0]));
	memcpy(y, b, d * sizeof(y[0]));
	for (k = 0; k < d-1; k++) {
		i = p[k];
		if (i != k)
			swap(y[k], y[i]);
		if (y[k] == 0)
			continue;
		for (i = k+1; i < d; i++)
			y[i] -= y[k] * u[i*d+k];
	}
	/* solve Ux=y */
	for (k = d; k > 0; k--) {
		s = y[k-1];
		for (i = k; i < d; i++)
			s -= u[(k-1)*d+i] * x[i];
		x[k-1] = s / u[(k-1)*d+(k-1)];
	}
}

template<typename T> void minv(T *ainv, const T *a, unsigned int d)
{
	T *u, *y;
	unsigned int *p;
	unsigned int k;

	u = alloca(d * d * sizeof(u[0]));
	p = alloca((d-1) * sizeof(p[0]));
	mlufact(u, p, a, d);
	for (k = 0; k < d; k++) {
		y = &ainv[k*d];
		memset(y, 0, d * sizeof(y[0]));
		y[k] = T(1);
		mlusolve(y, y, u, p, d);
	}
	mtranspose(ainv, ainv, d, d);
}

/*
 * Gauss-Seidel iterative solver
 */

/*
 * This routine calculates A*c=b iteratively, using the Gauss-Seidel iteration method
 */

template<typename T> void mgaussseidel(const complex<T> *a, const complex<T> *b, complex<T> *c, unsigned int d, unsigned int iter)
{
	complex<T> *adiag;
	unsigned int i, j, k;
	complex<T> s;

	adiag = alloca(d*sizeof(adiag[0]));
	/* initial vector */
	for (i = 1; i < d; i++)
		c[i] = T(0);
	c[0] = T(1);
	/* check for diagonal */
	for (i = 0; i < d; i++) {
		if (a[i*d+i] == T(0)) {
			ostringstream os;
			os << "mgaussseidel: matrix diagonal zero a[" << i << "][" << i << "]=" << a[i*d+i] << "\n";
			throw matrix_exception(os.str());
		}
		adiag[i] = T(1) / a[i*d+i];
	}
	/* iteration */
	for (i = 0; i < iter; i++) {
		for (j = 0; j < d; j++) {
			s = b[j];
			for (k = 0; k < j; k++)
				s -= a[j*d+k] * c[k];
			for (k = j+1; k < d; k++)
				s -= a[j*d+k] * c[k];
			c[j] = s * adiag[j];
		}
	}
}

#endif /* _MAT_H */
