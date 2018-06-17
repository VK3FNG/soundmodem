INCLUDES = -I$(top_srcdir)/soundcard -I$(top_srcdir)/libmisc -I$(top_srcdir)/matlib

noinst_LIBRARIES = libp3d.a
#dnl sbin_PROGRAMS

genp3dtbl_SOURCES = genp3dtbl.c
genp3dtbl_LDADD = ../matlib/libmat.a

testcrc_SOURCES = testcrc.c

libp3d_a_SOURCES = p3dmodem.c

noinst_HEADERS = \
	p3d.h \
	p3dtbl.h

if CROSSCOMP

else

noinst_PROGRAMS = genp3dtbl testcrc

$(srcdir)/p3dtbl.h:	genp3dtbl
			./genp3dtbl > $@

endif

EXTRA_DIST = 
