INCLUDES = -I$(top_srcdir)/soundcard -I$(top_srcdir)/libmisc -I$(top_srcdir)/matlib

noinst_LIBRARIES = libpsk.a
#dnl sbin_PROGRAMS

measpsk_SOURCES = measpsk.c
measpsk_LDADD = ../libmisc/libmisc.a ../matlib/libmat.a

genpsktbl_SOURCES = genpsktbl.c
genpsktbl_LDADD = ../matlib/libmat.a

libpsk_a_SOURCES = pskmodem.c pskmlse.c

noinst_HEADERS = \
	meas.h \
	psk.h \
	psktbl.h

if WIN32
MEASPROG =
else
MEASPROG = measpsk
endif

if CROSSCOMP

else

noinst_PROGRAMS = $(MEASPROG) genpsktbl

$(srcdir)/psktbl.h:	genpsktbl$(EXEEXT)
			./genpsktbl > $@

endif

