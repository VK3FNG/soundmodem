## Process this file with automake to produce Makefile.in

INCLUDES = @GTK_CFLAGS@ @XML_CFLAGS@ -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/matlib -I$(top_srcdir)/libmisc \
	-I$(top_srcdir)/soundcard -I$(top_srcdir)/directx/include -I$(top_srcdir)/directx/include/directx6 \
	-I$(top_srcdir)/configapp

man_MANS = soundmodemconfig.8

if WIN32
#MWINDOWS = -mwindows
MWINDOWS =
RCOBJ = configapp.res
else
MWINDOWS =
RCOBJ =
endif

MODEMLIBS = ../../afsk/libafsk.a ../../fsk/libfsk.a ../../pammodem/libpam.a ../../pskmodem/libpsk.a ../../newqpsk/libnewqpsk.a ../../p3dmodem/libp3d.a

bin_PROGRAMS = soundmodemconfig

if WIN32
configapp.res:	configapp.rc
		$(WINDRES) $< -O coff -o $@
endif

soundmodemconfig_SOURCES = \
	main.c app.c diag.c configapp.h \
	support.c support.h \
	interface.c interface.h \
	callbacks.h \
	spectrum.c spectrum.h \
	scope.c scope.h \
	snm-compat-gtk2.h

soundmodemconfig_LDADD = ../../soundcard/liblog.a ../../soundcard/libaudioio.a ../../soundcard/libpttio.a \
			$(MODEMLIBS) ../../matlib/libmat.a ../../libmisc/libmisc.a ../../soundcard/libcommon.a \
			@LIBTHREAD@ @GTK_LIBS@ @XML_LIBS@ @AUDIOFILE_LIBS@ @ALSA_LIBS@ $(RCOBJ)
soundmodemconfig_LDFLAGS = $(MWINDOWS)

EXTRA_DIST = configapp.rc soundmodemconfig.8
