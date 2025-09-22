## Process this file with automake to produce Makefile.in
# Copyright (C) 2002-2022 Free Software Foundation, Inc.
#
# This file is part of LIBTASN1.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)
AM_CPPFLAGS = -I$(top_builddir)/lib/includes -I$(top_srcdir)/lib/includes

EXTRA_DIST = asn1Coding_test.asn asn1Coding_test.asg

LDADD = ../lib/libtasn1.la

noinst_PROGRAMS = CertificateExample CrlExample

CertificateExample_SOURCES = CertificateExample.c pkix_asn1_tab.c pkix.asn

CrlExample_SOURCES = CrlExample.c pkix_asn1_tab.c pkix.asn

BUILT_SOURCES = pkix_asn1_tab.c
MAINTAINERCLEANFILES = pkix_asn1_tab.c

pkix_asn1_tab.c: $(srcdir)/pkix.asn
	../src/asn1Parser -o $@ $(srcdir)/pkix.asn
