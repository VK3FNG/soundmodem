INCLUDES = -I$(top_srcdir)/soundcard

noinst_LIBRARIES = libnewqpsk.a

libnewqpsk_a_SOURCES = \
	modulator.c newqpsktx.c \
	demodulator.c newqpskrx.c \
	fec.c bch.c tbl.c filter.c

noinst_HEADERS = \
	bch.h complex.h fec.h fectable.h filter-i386.h filter.h \
	misc.h modemconfig.h newqpskrx.h newqpsktx.h tbl.h

if CROSSCOMP
else

noinst_PROGRAMS = genfilt
genfilt_SOURCES = genfilt.c
# BUILT_SOURCES = filter-i386.h

filter-i386.h:	genfilt$(EXEEXT)
		./genfilt > filter-i386.h

endif

EXTRA_DIST = README.newqpsk
