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

AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS) $(CFLAG_VISIBILITY)
AM_CPPFLAGS = -I$(top_builddir)/lib/includes -I$(top_srcdir)/lib/includes -I$(top_srcdir)/lib

AM_LDFLAGS = -no-install
LDADD = ../lib/libtasn1.la

BUILT_SOURCES = CVE-2018-1000654-1_asn1_tab.h CVE-2018-1000654-2_asn1_tab.h
EXTRA_DIST = Test_parser.asn Test_tree.asn Test_tree_asn1_tab.c	mscat.asn \
	Test_encoding.asn pkix.asn TestIndef.p12 choice.asn coding-decoding2.asn \
	TestIndef2.p12 TestIndef3.der TestCertOctetOverflow.der \
	cert-invalid-time.der setof.asn CVE-2018-1000654-1.asn CVE-2018-1000654-2.asn \
	libtasn1.supp ocsp-basic-response.der spc_pe_image_data.der \
	invalid-x509/id-000000.der invalid-x509/id-000001.der \
	invalid-x509/id-000002.der invalid-x509/id-000003.der \
	invalid-x509/id-000004.der invalid-x509/id-000005.der \
	invalid-x509/id-000006.der invalid-x509/id-000007.der \
	invalid-x509/id-000008.der invalid-x509/id-000009.der \
	invalid-x509/id-000010.der invalid-x509/id-000011.der \
	invalid-x509/id-000012.der invalid-x509/id-000013.der \
	invalid-x509/id-000014.der invalid-x509/id-000015.der \
	invalid-x509/id-000016.der invalid-x509/id-000017.der \
	invalid-x509/id-000018.der invalid-x509/id-000019.der \
	invalid-x509/id-000020.der invalid-x509/id-000021.der \
	invalid-x509/id-000022.der invalid-x509/id-000023.der \
	invalid-x509/id-000024.der invalid-x509/id-000025.der \
	invalid-x509/id-000026.der invalid-x509/id-000027.der \
	invalid-x509/id-000028.der invalid-x509/id-000029.der \
	invalid-x509/id-000030.der invalid-x509/id-000031.der \
	invalid-x509/id-000032.der invalid-x509/id-000033.der \
	invalid-x509/id-000034.der invalid-x509/id-000035.der \
	invalid-pkcs7/id-000001.der invalid-pkcs7/id-000002.der \
	invalid-pkcs7/id-000003.der invalid-assignments1.txt \
	invalid-assignments2.txt pkix.asn.out

# For crlf.
EXTRA_DIST += crlf.cer crl.der ocsp.der
dist_check_SCRIPTS = crlf.sh benchmark.sh threadsafety.sh decoding.sh decoding-invalid-x509.sh \
	decoding-invalid-pkcs7.sh coding.sh parser.sh

TEST_EXTENSIONS = .sh

MOSTLYCLEANFILES = Test_parser_ERROR.asn CVE-2018-1000654-1_asn1_tab.h CVE-2018-1000654-2_asn1_tab.h

ctests = Test_parser Test_tree Test_encoding Test_indefinite		\
	Test_errors Test_simple Test_overflow Test_strings		\
	Test_choice Test_encdec copynode coding-decoding2 strict-der	\
	Test_choice_ocsp ocsp-basic-response octet-string		\
	coding-long-oid object-id-decoding spc_pe_image_data setof	\
	CVE-2018-1000654 reproducers object-id-encoding version

check_PROGRAMS = $(ctests)
TESTS = $(ctests) crlf.sh threadsafety.sh decoding.sh			\
	decoding-invalid-x509.sh decoding-invalid-pkcs7.sh coding.sh	\
	parser.sh

CVE-2018-1000654-1_asn1_tab.h: $(srcdir)/CVE-2018-1000654-1.asn
	$(top_builddir)/src/asn1Parser$(EXEEXT) $^ -o $@

CVE-2018-1000654-2_asn1_tab.h: $(srcdir)/CVE-2018-1000654-2.asn
	$(top_builddir)/src/asn1Parser$(EXEEXT) $^ -o $@

CVE_2018_1000654_SOURCES = CVE-2018-1000654-1_asn1_tab.h CVE-2018-1000654-2_asn1_tab.h CVE-2018-1000654.c

LOG_COMPILER = $(VALGRIND)
SH_LOG_COMPILER= env VALGRIND="$(VALGRIND)" LIBTOOL="$(LIBTOOL)"

TESTS_ENVIRONMENT = \
	ASN1PARSER=$(srcdir)/Test_parser.asn \
	ASN1TREE=$(srcdir)/Test_tree.asn \
	ASN1CHOICE=$(srcdir)/choice.asn \
	ASN1CODINGDECODING2=$(srcdir)/coding-decoding2.asn \
	ASN1PKIX=$(srcdir)/pkix.asn \
	ASN1SETOF=$(srcdir)/setof.asn \
	ASN1CRLDER=$(srcdir)/crl.der \
	ASN1INDEF=$(srcdir)/TestIndef.p12 \
	ASN1INDEF2=$(srcdir)/TestIndef2.p12 \
	ASN1INDEF3=$(srcdir)/TestIndef3.der \
	ASN1ENCODING=$(srcdir)/Test_encoding.asn \
	ASN1CHOICE_OCSP=$(srcdir)/pkix.asn \
	ASN1CHOICE_OCSP_DATA=$(srcdir)/ocsp.der \
	ASN1_RESPONSE_OCSP_DATA=$(srcdir)/ocsp-basic-response.der \
	ASN1_MSCAT=$(srcdir)/mscat.asn \
	ASN1_SPC_PE_IMAGE_DATA=$(srcdir)/spc_pe_image_data.der \
	ASAN_OPTIONS="detect_leaks=0:exitcode=6" \
	THREADSAFETY_FILES=`find $(top_srcdir)/lib -name \*.c` \
	EXEEXT=$(EXEEXT) \
	LSAN_OPTIONS=suppressions=libtasn1.supp
