ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	exports

SUBDIRS = po

AM_CPPFLAGS = \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\"

pkgconfdir = $(libdir)/pkgconfig

pkgconf_DATA = libattr.pc
pkginclude_HEADERS =
bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_HEADERS =
noinst_LTLIBRARIES =
dist_doc_DATA =
dist_man_MANS =
dist_sysconf_DATA = xattr.conf

update-po:
	$(MAKE) -C po $@

include doc/Makemodule.am
include examples/Makemodule.am
include include/Makemodule.am
include libattr/Makemodule.am
include libmisc/Makemodule.am
include man/Makemodule.am
include test/Makemodule.am
include tools/Makemodule.am
