INCLUDES = @XML_CFLAGS@ -I$(top_srcdir)/matlib -I$(top_srcdir)/libmisc \
        -I$(top_srcdir)/directx/include \
        -I$(top_srcdir)/directx/include/directx6 \
	@AUDIOFILE_CFLAGS@

MODEMLIBS = ../afsk/libafsk.a ../fsk/libfsk.a ../pammodem/libpam.a ../pskmodem/libpsk.a ../newqpsk/libnewqpsk.a ../p3dmodem/libp3d.a

man_MANS = soundmodem.8

if WIN32
SBINPROG = 
NOINSTPROG = 
libpttio_a_SOURCES = winptt2.c
libaudioio_a_SOURCES = winaudioio.c audiofilein.c chansim.c
liblog_a_SOURCES = winlog.c
else
SBINPROG = soundmodem
NOINSTPROG = testkiss chansim chansimberr
libpttio_a_SOURCES = ptt.c
libaudioio_a_SOURCES = audioio.c alsaio.c audiofilein.c chansim.c
liblog_a_SOURCES = log.c
endif

include_HEADERS = modem.h simd.h
noinst_HEADERS = ppdev.h soundio.h audioio.h kisspkt.h pttio.h

sbin_PROGRAMS = $(SBINPROG)
noinst_PROGRAMS = $(NOINSTPROG)
noinst_LIBRARIES = libcommon.a libpttio.a libaudioio.a liblog.a

libcommon_a_SOURCES = simd.c snprintpkt.c

soundmodem_SOURCES = kisspkt.c main.c
soundmodem_LDADD = $(MODEMLIBS) libcommon.a liblog.a libaudioio.a libpttio.a ../matlib/libmat.a ../libmisc/libmisc.a @XML_LIBS@ @LIBTHREAD@ @AUDIOFILE_LIBS@

testkiss_SOURCES = kisspkt.c testkiss.c log.c snprintpkt.c
testkiss_LDADD = ../libmisc/libmisc.a @LIBTHREAD@

chansim_SOURCES = simd.c pktsimple.c main.c
chansim_LDADD = $(MODEMLIBS) liblog.a libaudioio.a libpttio.a ../matlib/libmat.a ../libmisc/libmisc.a @XML_LIBS@ @LIBTHREAD@ @AUDIOFILE_LIBS@

chansimberr_SOURCES = simd.c pktberr.c main.c
chansimberr_LDADD = $(MODEMLIBS) liblog.a libaudioio.a libpttio.a ../matlib/libmat.a ../libmisc/libmisc.a @XML_LIBS@ @LIBTHREAD@ @AUDIOFILE_LIBS@

EXTRA_DIST = test.xml audioio.c audiofilein.c chansim.c winaudioio.c ptt.c winptt.c winptt2.c log.c winlog.c soundmodem.8
