## Process this file with automake to produce Makefile.in

## Copyright (C) 2010-2020 Colin Watson.
##
## This file is part of libpipeline.
##
## libpipeline is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or (at
## your option) any later version.
##
## libpipeline is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with libpipeline; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
## USA

lib_LTLIBRARIES = libpipeline.la

AM_CFLAGS = $(WARN_CFLAGS)

libpipeline_la_CPPFLAGS = \
	-I$(top_srcdir)/gl/lib \
	-I$(top_builddir)/gl/lib

libpipeline_la_SOURCES = \
	appendstr.c \
	debug.c \
	fatal.c \
	fatal.h \
	pipeline.c \
	pipeline-private.h

include_HEADERS = pipeline.h

libpipeline_la_LIBADD = ../gl/lib/libgnu.la $(LTLIBOBJS)

libpipeline_la_LDFLAGS = \
	-export-symbols-regex '^(pipecmd|pipeline)_' \
	-no-undefined \
	-version-info 6:7:5

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpipeline.pc
