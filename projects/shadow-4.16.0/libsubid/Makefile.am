lib_LTLIBRARIES = libsubid.la
libsubid_la_SOURCES = api.c
libsubid_la_LDFLAGS = -version-info @LIBSUBID_ABI_MAJOR@ -export-symbols-regex '^subid_'

pkginclude_HEADERS = subid.h

MISCLIBS = \
	$(LIBAUDIT) \
	$(LIBSELINUX) \
	$(LIBSEMANAGE) \
	$(LIBCRYPT_NOPAM) \
	$(LIBSKEY) \
	$(LIBMD) \
	$(LIBECONF) \
	$(LIBCRYPT) \
	$(LIBACL) \
	$(LIBATTR) \
	$(LIBTCB) \
	$(LIBPAM)

libsubid_la_LIBADD = \
	$(top_builddir)/lib/libshadow.la \
	$(MISCLIBS) -ldl

AM_CPPFLAGS = \
	-I${top_srcdir}/lib \
	-DLOCALEDIR=\"$(datadir)/locale\"
