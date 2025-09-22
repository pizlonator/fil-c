## Makefile.am for libdwelf library subdirectory in elfutils.
##
## Process this file with automake to create Makefile.in
##
## Copyright (C) 2014, 2015 Red Hat, Inc.
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
AM_CPPFLAGS += -I$(srcdir)/../libelf -I$(srcdir)/../libdw \
	       -I$(srcdir)/../libdwfl -I$(srcdir)/../libebl
VERSION = 1

noinst_LIBRARIES = libdwelf.a libdwelf_pic.a

pkginclude_HEADERS = libdwelf.h
noinst_HEADERS = libdwelfP.h

libdwelf_a_SOURCES = dwelf_elf_gnu_debuglink.c dwelf_dwarf_gnu_debugaltlink.c \
		     dwelf_elf_gnu_build_id.c dwelf_scn_gnu_compressed_size.c \
		     dwelf_strtab.c dwelf_elf_begin.c \
		     dwelf_elf_e_machine_string.c

libdwelf = $(libdw)

libdw = ../libdw/libdw.so
libelf = ../libelf/libelf.so
libebl = ../libebl/libebl.a
libeu = ../lib/libeu.a

libdwelf_pic_a_SOURCES =
am_libdwelf_pic_a_OBJECTS = $(libdwelf_a_SOURCES:.c=.os)

CLEANFILES += $(am_libdwelf_pic_a_OBJECTS)
