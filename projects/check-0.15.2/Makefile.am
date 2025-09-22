## Process this file with automake to produce Makefile.in

## run tests after everything else

if MAKE_DOCS
OPTIONAL_DOC_TARGET = doc
else
OPTIONAL_DOC_TARGET =
endif

SUBDIRS = lib src $(OPTIONAL_DOC_TARGET) . checkmk tests

## FIXME: maybe we don't need this line

AM_MAKEINFOFLAGS = -I$(top_srcdir)/doc/example

## what to clean

CLEANFILES = *~\
	$(PACKAGE)-$(VERSION).tar.gz\
	ChangeLog.bak

## what additional things to distribute

include_HEADERS = check_stdint.h

EXTRA_DIST = check.pc.in $(m4data_DATA) xml/check_unittest.xslt \
	CMakeLists.txt src/CMakeLists.txt tests/CMakeLists.txt lib/CMakeLists.txt \
	checkmk/CMakeLists.txt \
	cmake

## install docs
docdir = $(datadir)/doc/$(PACKAGE)
doc_DATA = ChangeLog NEWS README COPYING.LESSER

## install check.m4 with AM_PATH_CHECK in it
m4datadir = $(datadir)/aclocal
m4data_DATA = check.m4

## install check.pc
pcdatadir = $(libdir)/pkgconfig
pcdata_DATA = check.pc

DISTCLEANFILES = check_stdint.h

ACLOCAL_AMFLAGS = -I m4

README:
	fgrep -v "Build Status]" $(top_srcdir)/README.md > $@

doc/check_html:
	$(MAKE) -C doc check_html

doc/doxygen:
	$(MAKE) -C doc doxygen

doc/doxygen-devel:
	$(MAKE) -C doc doxygen-devel

# check we can do a clean build, including docs.
# perhaps we should check for out of date (svn st -u) and modified files.
prereleasecheck: doc/check_html doc/doxygen
	-$(MAKE) distclean
	autoreconf -i && ./configure \
	    && ulimit -c 0 && \
	    $(MAKE) distcheck

.PHONY: prereleasecheck
