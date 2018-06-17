
INCLUDES = -DCONFIGDIR=\"$(sysconfdir)\" -DLOCALSTATEDIR=\"$(localstatedir)\" \
	-I$(top_srcdir)/directx/include -I$(top_srcdir)/directx/include/directx6

if DIRECTX
BUILT_SOURCES = libddraw.a libdsound.a libdinput.a libdplayx.a

noinst_LIBRARIES = libdinput1.a
libdinput1_a_SOURCES = dinput.c

endif

libddraw.a libdsound.a libdplayx.a: libddraw.def libdsound.def libdplayx.def
	for i in ddraw dsound dplayx; do \
	  $(DLLTOOL) --as $(AS) -k --dllname $$i.dll --output-lib lib$$i.a --def $(srcdir)/lib$$i.def; \
	done

libdinput.a: libdinput1.a libdinput.def
	$(DLLTOOL) --as $(AS) -k --dllname dinput.dll --output-lib $@ --def $(srcdir)/libdinput.def
	$(AR) xv libdinput1.a dinput.$(OBJEXT)
	$(AR) rv $@ dinput.$(OBJEXT)
	$(RM) -f $<

EXTRA_DIST = libddraw.def libdinput.def libdplayx.def libdsound.def dinput.c \
	fixdirectx.sh deunion.pl include/directx.h

# include/mmsystem.h include/objbase.h

CLEANFILES = libddraw.a libdinput.a libdplayx.a libdsound.a
