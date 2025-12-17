## Process this file with automake to produce Makefile.in
# Copyright (C) 2004-2012 Free Software Foundation, Inc.
# Copyright (C) 2024 Red Hat, Inc.
#
# Author: Daiki Ueno
#
# This file is part of GNUTLS.
#
# The GNUTLS library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 3 of
# the License, or (at your option) any later version.
#
# The GNUTLS library is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>

include $(top_srcdir)/lib/common.mk

AM_CFLAGS += $(LIBOQS_CFLAGS)

AM_CPPFLAGS = \
	-I$(srcdir)/../../gl		\
	-I$(builddir)/../../gl		\
	-I$(srcdir)/../includes		\
	-I$(builddir)/../includes	\
	-I$(srcdir)/..

if ENABLE_DLOPEN
AM_CPPFLAGS += $(LIBOQS_CFLAGS) -DGNUTLS_OQS_ENABLE_DLOPEN=1
endif

noinst_LTLIBRARIES = libcrypto.la

libcrypto_la_SOURCES = liboqs.h liboqs.c rand.h rand.c sha3.h sha3.c

if NEED_LIBOQS_SHA3_OPS_H
libcrypto_la_SOURCES += backport/sha3_ops.h
endif
