## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 1996-2001, 2002, 2005, 2019-2020 Red Hat, Inc.
## This file is part of elfutils.
##
## This file is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## elfutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
EXTRA_DIST = COPYING-GFDL README
dist_man1_MANS=readelf.1 elfclassify.1 srcfiles.1
notrans_dist_man3_MANS=elf_update.3 elf_getdata.3 elf_clone.3 elf_begin.3
notrans_dist_man7_MANS=
notrans_dist_man8_MANS=
notrans_dist_man1_MANS=

if DEBUGINFOD
if !LIBDEBUGINFOD
notrans_dist_man7_MANS += debuginfod-client-config.7
endif
notrans_dist_man8_MANS += debuginfod.8 debuginfod.service.8
endif

if LIBDEBUGINFOD
notrans_dist_man7_MANS += debuginfod-client-config.7
notrans_dist_man3_MANS += debuginfod_add_http_header.3
notrans_dist_man3_MANS += debuginfod_begin.3
notrans_dist_man3_MANS += debuginfod_end.3
notrans_dist_man3_MANS += debuginfod_find_debuginfo.3
notrans_dist_man3_MANS += debuginfod_find_executable.3
notrans_dist_man3_MANS += debuginfod_find_source.3
notrans_dist_man3_MANS += debuginfod_find_section.3
notrans_dist_man3_MANS += debuginfod_get_user_data.3
notrans_dist_man3_MANS += debuginfod_get_url.3
notrans_dist_man3_MANS += debuginfod_set_progressfn.3
notrans_dist_man3_MANS += debuginfod_set_user_data.3
notrans_dist_man1_MANS += debuginfod-find.1
endif
