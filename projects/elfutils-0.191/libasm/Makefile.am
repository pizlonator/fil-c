## Process this file with automake to create Makefile.in
##
## Copyright (C) 2002-2010 Red Hat, Inc.
## This file is part of elfutils.
##
## This file is free software; you can redistribute it and/or modify
## it under the terms of either
##
##   * the GNU Lesser General Public License as published by the Free
##     Software Foundation; either version 3 of the License, or (at
##     your option) any later version
##
## or
##
##   * the GNU General Public License as published by the Free
##     Software Foundation; either version 2 of the License, or (at
##     your option) any later version
##
## or both in parallel, as here.
##
## elfutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received copies of the GNU General Public License and
## the GNU Lesser General Public License along with this program.  If
## not, see <http://www.gnu.org/licenses/>.
##
include $(top_srcdir)/config/eu.am
AM_CPPFLAGS += -I$(top_srcdir)/libelf -I$(top_srcdir)/libebl -I$(top_srcdir)/libdw -I$(top_srcdir)/libdwelf

VERSION = 1

lib_LIBRARIES = libasm.a
noinst_LIBRARIES = libasm_pic.a
noinst_DATA = $(noinst_LIBRARIES:_pic.a=.so)
pkginclude_HEADERS = libasm.h

libasm_a_SOURCES = asm_begin.c asm_abort.c asm_end.c asm_error.c \
		   asm_getelf.c asm_newscn.c asm_newscn_ingrp.c \
		   asm_newsubscn.c asm_newsym.c asm_newcomsym.c \
		   asm_newabssym.c \
		   asm_newscngrp.c asm_scngrp_newsignature.c \
		   asm_fill.c asm_align.c asm_addstrz.c \
		   asm_addint8.c asm_adduint8.c \
		   asm_addint16.c asm_adduint16.c \
		   asm_addint32.c asm_adduint32.c \
		   asm_addint64.c asm_adduint64.c \
		   asm_adduleb128.c asm_addsleb128.c \
		   disasm_begin.c disasm_cb.c disasm_end.c disasm_str.c \
		   symbolhash.c

libasm_pic_a_SOURCES =
am_libasm_pic_a_OBJECTS = $(libasm_a_SOURCES:.c=.os)

libasm_so_DEPS = ../lib/libeu.a ../libebl/libebl_pic.a ../libelf/libelf.so ../libdw/libdw.so
libasm_so_LDLIBS = $(libasm_so_DEPS)
if USE_LOCKS
libasm_so_LDLIBS += -lpthread
endif

libasm_so_LIBS = libasm_pic.a
libasm.so: $(srcdir)/libasm.map $(libasm_so_LIBS) $(libasm_so_DEPS)
	$(AM_V_CCLD)$(LINK) $(dso_LDFLAGS) -o $@ \
		-Wl,--soname,$@.$(VERSION) \
		-Wl,--version-script,$< \
		$(NO_UNDEFINED) \
		-Wl,--whole-archive $(libasm_so_LIBS) -Wl,--no-whole-archive \
		$(libasm_so_LDLIBS)
	@$(textrel_check)
	$(AM_V_at)ln -fs $@ $@.$(VERSION)

install: install-am libasm.so
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	$(INSTALL_PROGRAM) libasm.so $(DESTDIR)$(libdir)/libasm-$(PACKAGE_VERSION).so
	ln -fs libasm-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/libasm.so.$(VERSION)
	ln -fs libasm.so.$(VERSION) $(DESTDIR)$(libdir)/libasm.so

uninstall: uninstall-am
	rm -f $(DESTDIR)$(libdir)/libasm-$(PACKAGE_VERSION).so
	rm -f $(DESTDIR)$(libdir)/libasm.so.$(VERSION)
	rm -f $(DESTDIR)$(libdir)/libasm.so
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(includedir)/elfutils

noinst_HEADERS = libasmP.h symbolhash.h
EXTRA_DIST = libasm.map

CLEANFILES += $(am_libasm_pic_a_OBJECTS) libasm.so libasm.so.$(VERSION)
