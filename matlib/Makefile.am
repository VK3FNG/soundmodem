#dnl INCLUDES = -I$(top_srcdir)

noinst_LIBRARIES = libmat.a

libmat_a_SOURCES = \
	randn.c \
	dcchol.c \
	drchol.c \
	fcchol.c \
	frchol.c \
	fcgseidel.c \
	frgseidel.c \
	frtransp.c \
	fradd.c \
	frsub.c \
	frmul.c \
	frinv.c \
	frdet.c \
	fctransp.c \
	fchermtransp.c \
	fcconj.c \
	fcadd.c \
	fcsub.c \
	fcmul.c \
	fcinv.c \
	fcdet.c

noinst_HEADERS = \
	mat.h \
	fft.hh \
	mat.hh

noinst_PROGRAMS = tests

tests_SOURCES = tests.c
tests_LDADD = libmat.a ../libmisc/libmisc.a

#dnl EXTRA_DIST = bpskberr.m
