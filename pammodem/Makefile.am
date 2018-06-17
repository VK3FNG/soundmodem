INCLUDES = -I$(top_srcdir)/soundcard -I$(top_srcdir)/libmisc -I$(top_srcdir)/matlib

noinst_LIBRARIES = libpam.a
#dnl sbin_PROGRAMS

libpam_a_SOURCES = pammodem.c

meas_SOURCES = meas.c
meas_LDADD = ../libmisc/libmisc.a ../matlib/libmat.a

genpamtbl_SOURCES = genpamtbl.c
genpamtbl_LDADD = ../matlib/libmat.a

noinst_HEADERS = \
	meas.h \
	pam.h \
	pamtbl.h

if WIN32
MEASPROG =
else
MEASPROG = meas
endif

if CROSSCOMP

else

noinst_PROGRAMS = $(MEASPROG) genpamtbl

$(srcdir)/pamtbl.h:	genpamtbl$(EXEEXT)
			./genpamtbl > $@

endif
