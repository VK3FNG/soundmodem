INCLUDES = -I$(top_srcdir)/soundcard

noinst_LIBRARIES = libafsk.a
libafsk_a_SOURCES = modem.c costab.c

noinst_HEADERS = costab.h

gentbl_SOURCES = gentbl.c

if CROSSCOMP

else

noinst_PROGRAMS = gentbl

$(srcdir)/costab.c:	gentbl$(EXEEXT)
			bdir=`pwd`; cd $(srcdir); $$bdir/gentbl

endif
