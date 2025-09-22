## Process this file with automake to create Makefile.in
##
## Copyright (C) 1996-2010, 2015 Red Hat, Inc.
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
if BUILD_STATIC
AM_CFLAGS += $(fpic_CFLAGS)
endif

VERSION = 1

lib_LIBRARIES = libelf.a
noinst_LIBRARIES = libelf_pic.a
noinst_DATA = $(noinst_LIBRARIES:_pic.a=.so)
include_HEADERS = libelf.h gelf.h nlist.h

noinst_HEADERS = abstract.h common.h exttypes.h gelf_xlate.h libelfP.h \
		 version_xlate.h gnuhash_xlate.h note_xlate.h dl-hash.h \
		 chdr_xlate.h elf32_getchdr.h

if INSTALL_ELFH
include_HEADERS += elf.h
else
noinst_HEADERS += elf.h
endif

pkginclude_HEADERS = elf-knowledge.h

libelf_a_SOURCES = elf_version.c elf_hash.c elf_error.c elf_fill.c \
		   elf_begin.c elf_next.c elf_rand.c elf_end.c elf_kind.c \
		   gelf_getclass.c elf_getbase.c elf_getident.c \
		   elf32_fsize.c elf64_fsize.c gelf_fsize.c \
		   elf32_xlatetof.c elf32_xlatetom.c elf64_xlatetof.c \
		   elf64_xlatetom.c gelf_xlate.c \
		   elf32_getehdr.c elf64_getehdr.c gelf_getehdr.c \
		   elf32_newehdr.c elf64_newehdr.c gelf_newehdr.c \
		   gelf_update_ehdr.c \
		   elf32_getphdr.c elf64_getphdr.c gelf_getphdr.c \
		   elf32_newphdr.c elf64_newphdr.c gelf_newphdr.c \
		   gelf_update_phdr.c \
		   elf_getarhdr.c elf_getarsym.c \
		   elf_rawfile.c elf_readall.c elf_cntl.c \
		   elf_getscn.c elf_nextscn.c elf_ndxscn.c elf_newscn.c \
		   elf32_getshdr.c elf64_getshdr.c gelf_getshdr.c \
		   gelf_update_shdr.c \
		   elf_strptr.c elf_rawdata.c elf_getdata.c elf_newdata.c \
		   elf_getdata_rawchunk.c \
		   elf_flagelf.c elf_flagehdr.c elf_flagphdr.c elf_flagscn.c \
		   elf_flagshdr.c elf_flagdata.c elf_memory.c \
		   elf_update.c elf32_updatenull.c elf64_updatenull.c \
		   elf32_updatefile.c elf64_updatefile.c \
		   gelf_getsym.c gelf_update_sym.c \
		   gelf_getversym.c gelf_getverneed.c gelf_getvernaux.c \
		   gelf_getverdef.c gelf_getverdaux.c \
		   gelf_getrel.c gelf_getrela.c \
		   gelf_update_rel.c gelf_update_rela.c \
		   gelf_getdyn.c gelf_update_dyn.c \
		   gelf_getmove.c gelf_update_move.c \
		   gelf_getsyminfo.c gelf_update_syminfo.c \
		   gelf_getauxv.c gelf_update_auxv.c \
		   gelf_getnote.c \
		   gelf_xlatetof.c gelf_xlatetom.c \
		   nlist.c \
		   gelf_getsymshndx.c gelf_update_symshndx.c \
		   gelf_update_versym.c gelf_update_verneed.c \
		   gelf_update_vernaux.c gelf_update_verdef.c \
		   gelf_update_verdaux.c \
		   elf_getphdrnum.c elf_getshdrnum.c elf_getshdrstrndx.c \
		   gelf_checksum.c elf32_checksum.c elf64_checksum.c \
		   libelf_crc32.c libelf_next_prime.c \
		   elf_clone.c \
		   gelf_getlib.c gelf_update_lib.c \
		   elf32_offscn.c elf64_offscn.c gelf_offscn.c \
		   elf_getaroff.c \
		   elf_gnu_hash.c \
		   elf_scnshndx.c \
		   elf32_getchdr.c elf64_getchdr.c gelf_getchdr.c \
		   elf_compress.c elf_compress_gnu.c

libelf_pic_a_SOURCES =
am_libelf_pic_a_OBJECTS = $(libelf_a_SOURCES:.c=.os)

libelf_so_DEPS = ../lib/libeu.a
libelf_so_LDLIBS = $(libelf_so_DEPS) -lz $(zstd_LIBS)
if USE_LOCKS
libelf_so_LDLIBS += -lpthread
endif

libelf_so_LIBS = libelf_pic.a
libelf.so: $(srcdir)/libelf.map $(libelf_so_LIBS) $(libelf_so_DEPS)
	$(AM_V_CCLD)$(LINK) $(dso_LDFLAGS) -o $@ \
		-Wl,--soname,$@.$(VERSION) \
		-Wl,--version-script,$< \
		$(NO_UNDEFINED) \
		-Wl,--whole-archive $(libelf_so_LIBS) -Wl,--no-whole-archive \
		$(libelf_so_LDLIBS)
	@$(textrel_check)
	$(AM_V_at)ln -fs $@ $@.$(VERSION)

install: install-am libelf.so
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	$(INSTALL_PROGRAM) libelf.so $(DESTDIR)$(libdir)/libelf-$(PACKAGE_VERSION).so
	ln -fs libelf-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/libelf.so.$(VERSION)
	ln -fs libelf.so.$(VERSION) $(DESTDIR)$(libdir)/libelf.so

uninstall: uninstall-am
	rm -f $(DESTDIR)$(libdir)/libelf-$(PACKAGE_VERSION).so
	rm -f $(DESTDIR)$(libdir)/libelf.so.$(VERSION)
	rm -f $(DESTDIR)$(libdir)/libelf.so

EXTRA_DIST = libelf.map

CLEANFILES += $(am_libelf_pic_a_OBJECTS) libelf.so libelf.so.$(VERSION)
