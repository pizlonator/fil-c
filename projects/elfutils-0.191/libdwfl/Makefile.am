## Makefile.am for libdwfl library subdirectory in elfutils.
##
## Process this file with automake to create Makefile.in
##
## Copyright (C) 2005-2010, 2013 Red Hat, Inc.
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
AM_CPPFLAGS += -I$(srcdir) -I$(srcdir)/../libelf -I$(srcdir)/../libebl \
	   -I$(srcdir)/../libdw -I$(srcdir)/../libdwelf -I$(builddir)/../debuginfod
VERSION = 1

noinst_LIBRARIES = libdwfl.a
noinst_LIBRARIES += libdwfl_pic.a

pkginclude_HEADERS = libdwfl.h


libdwfl_a_SOURCES = dwfl_begin.c dwfl_end.c dwfl_error.c dwfl_version.c \
		    dwfl_module.c dwfl_report_elf.c relocate.c \
		    dwfl_module_build_id.c dwfl_module_report_build_id.c \
		    derelocate.c offline.c segment.c \
		    dwfl_module_info.c dwfl_getmodules.c dwfl_getdwarf.c \
		    dwfl_module_getdwarf.c dwfl_module_getelf.c \
		    dwfl_validate_address.c \
		    argp-std.c find-debuginfo.c \
		    dwfl_build_id_find_elf.c \
		    dwfl_build_id_find_debuginfo.c \
		    linux-kernel-modules.c linux-proc-maps.c \
		    dwfl_addrmodule.c dwfl_addrdwarf.c \
		    cu.c dwfl_module_nextcu.c dwfl_nextcu.c dwfl_cumodule.c \
		    dwfl_module_addrdie.c dwfl_addrdie.c \
		    lines.c dwfl_lineinfo.c dwfl_line_comp_dir.c \
		    dwfl_linemodule.c dwfl_linecu.c dwfl_dwarf_line.c \
		    dwfl_getsrclines.c dwfl_onesrcline.c \
		    dwfl_module_getsrc.c dwfl_getsrc.c \
		    dwfl_module_getsrc_file.c \
		    libdwfl_crc32.c libdwfl_crc32_file.c \
		    elf-from-memory.c \
		    dwfl_module_dwarf_cfi.c dwfl_module_eh_cfi.c \
		    dwfl_module_getsym.c \
		    dwfl_module_addrname.c dwfl_module_addrsym.c \
		    dwfl_module_return_value_location.c \
		    dwfl_module_register_names.c \
		    dwfl_segment_report_module.c \
		    link_map.c core-file.c open.c image-header.c \
		    dwfl_frame.c frame_unwind.c dwfl_frame_pc.c \
		    linux-pid-attach.c linux-core-attach.c dwfl_frame_regs.c \
		    gzip.c debuginfod-client.c

if BZLIB
libdwfl_a_SOURCES += bzip2.c
endif
if LZMA
libdwfl_a_SOURCES += lzma.c
endif
if ZSTD
libdwfl_a_SOURCES += zstd.c
endif

libdwfl = $(libdw)
libdw = ../libdw/libdw.so
libelf = ../libelf/libelf.so
libebl = ../libebl/libebl.a
libeu = ../lib/libeu.a

libdwfl_pic_a_SOURCES =
am_libdwfl_pic_a_OBJECTS = $(libdwfl_a_SOURCES:.c=.os)

noinst_HEADERS = libdwflP.h

CLEANFILES += $(am_libdwfl_pic_a_OBJECTS)
