EXTRA_DIST = soundmodem.c soundmodem.rc driver.c drv32.h driver.obj flexdriv.def flexdrv.h resource.h

INCLUDES = -I$(top_srcdir)/intl -I$(top_srcdir) -I$(top_srcdir)/matlib -I$(top_srcdir)/libmisc \
	-I$(top_srcdir)/soundcard -I$(top_srcdir)/directx/include -I$(top_srcdir)/directx/include/directx6 \
	@PTHREAD_CFLAGS@


MODEMLIBS = ../afsk/libafsk.a ../fsk/libfsk.a ../pammodem/libpam.a ../pskmodem/libpsk.a ../newqpsk/libnewqpsk.a ../p3dmodem/libp3d.a

if WIN32

noinst_LIBRARIES = libflxdrv.a

libflxdrv_a_SOURCES = driver.c soundmodem.c

soundmodem.res:	soundmodem.rc resource.h
	        $(WINDRES) $< -O coff -o $@

DLLNAME		= soundmodem
DLLOBJ		= driver.o soundmodem.o soundmodem.res $(MODEMLIBS) ../soundcard/libcommon.a ../soundcard/libaudioio.a ../soundcard/libpttio.a \
			../matlib/libmat.a ../libmisc/libmisc.a
DLLLDARGS	= @PTHREAD_LIBS@ @LIBTHREAD@ $(LIBS)
DLLDEF		= --def $(srcdir)/flexdriv.def

noinst_SCRIPTS = $(DLLNAME).dll

$(DLLNAME).dll:	$(DLLOBJ)
	$(CC) -mdll -mno-cygwin -Wl,--base-file,$(DLLNAME).base -o $(DLLNAME).dll $(DLLOBJ) $(DLLLDARGS)
	$(DLLTOOL) --as=$(AS) --dllname $(DLLNAME).dll $(DLLDEF) --base-file $(DLLNAME).base --output-exp $(DLLNAME).exp $(DLLOBJ)
	$(CC) -mdll -mno-cygwin -Wl,--base-file,$(DLLNAME).base,$(DLLNAME).exp -o $(DLLNAME).dll $(DLLOBJ) $(DLLLDARGS)
	$(DLLTOOL) --as=$(AS) --dllname $(DLLNAME).dll $(DLLDEF) --base-file $(DLLNAME).base --output-exp $(DLLNAME).exp $(DLLOBJ)
	$(CC) -mdll -mno-cygwin -Wl,$(DLLNAME).exp -o $(DLLNAME).dll $(DLLOBJ) $(DLLLDARGS)
	$(DLLTOOL) --as=$(AS) --dllname $(DLLNAME).dll $(DLLDEF) --output-lib lib$(DLLNAME).a $(DLLOBJ)

endif
