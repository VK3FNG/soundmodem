INCLUDES = -I$(top_srcdir)/soundcard -I$(top_srcdir)/matlib -I$(top_srcdir)/libmisc -I.


noinst_LIBRARIES = libfsk.a
libfsk_a_SOURCES = fskic.c fskic.h modem.c modempsp.c psp.h modemeq.c

genpsptbl_SOURCES = genpsptbl.c
genpsptbl_LDADD = ../matlib/libmat.a

gendf9icfilt_SOURCES = gendf9icfilt.cc
gendf9icfilt_LDADD = ../matlib/libmat.a ../libmisc/libmisc.a

if CROSSCOMP

else

noinst_PROGRAMS = genpsptbl gendf9icfilt

$(srcdir)/psp.h:	genpsptbl$(EXEEXT)
			./genpsptbl$(EXEEXT) > $@

$(srcdir)/fskic.h:	gendf9icfilt$(EXEEXT)
			./gendf9icfilt$(EXEEXT) > $@

endif
