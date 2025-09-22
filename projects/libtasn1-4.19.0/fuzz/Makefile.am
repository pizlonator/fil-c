# Copyright(c) 2017-2019 Tim Ruehsen
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
# This file is part of libtasn1.

include $(top_srcdir)/aminclude_static.am

AM_CFLAGS = $(WERROR_CFLAGS) $(WARN_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_CPPFLAGS = -I$(top_builddir)/lib/includes -I$(top_srcdir)/lib/includes -I$(srcdir) \
 -I$(top_builddir)/lib/gl -I$(top_srcdir)/lib/gl -DSRCDIR=\"$(abs_srcdir)\" \
 $(CODE_COVERAGE_CPPFLAGS)
AM_LDFLAGS = -no-install

LDADD = ../lib/gl/libgnu.la ../lib/libtasn1.la $(FUZZ_LIBS) $(CODE_COVERAGE_LIBS)

FUZZERS = \
 libtasn1_array2tree_fuzzer$(EXEEXT) \
 libtasn1_parser2tree_fuzzer$(EXEEXT) \
 libtasn1_pkix_der_fuzzer$(EXEEXT) \
 libtasn1_gnutls_der_fuzzer$(EXEEXT) \
 asn1_get_length_ber_fuzzer$(EXEEXT) \
 asn1_get_length_der_fuzzer$(EXEEXT) \
 asn1_get_object_id_der_fuzzer$(EXEEXT) \
 asn1_decode_simple_ber_fuzzer$(EXEEXT) \
 asn1_decode_simple_der_fuzzer$(EXEEXT)

if FUZZING
  # fuzzing mode, enabled with ./configure --enable-fuzzing
  noinst_PROGRAMS = $(FUZZERS)
  AM_LDFLAGS += -fsanitize=fuzzer
  MAIN = fuzzer.h
else
  MAIN = main.c fuzzer.h
  AM_CPPFLAGS += -DTEST_RUN
  TESTS = $(FUZZERS)
  check_PROGRAMS = $(FUZZERS)

  noinst_PROGRAMS = corpus2array
  corpus2array_SOURCES = corpus2array.c
endif

libtasn1_array2tree_fuzzer_SOURCES = libtasn1_array2tree_fuzzer.c $(MAIN)
libtasn1_parser2tree_fuzzer_SOURCES = libtasn1_parser2tree_fuzzer.c $(MAIN)
libtasn1_pkix_der_fuzzer_SOURCES = libtasn1_pkix_der_fuzzer.c $(MAIN)
libtasn1_gnutls_der_fuzzer_SOURCES = libtasn1_gnutls_der_fuzzer.c $(MAIN)
asn1_get_length_ber_fuzzer_SOURCES = asn1_get_length_ber_fuzzer.c $(MAIN)
asn1_get_length_der_fuzzer_SOURCES = asn1_get_length_der_fuzzer.c $(MAIN)
asn1_get_object_id_der_fuzzer_SOURCES = asn1_get_object_id_der_fuzzer.c $(MAIN)
asn1_decode_simple_der_fuzzer_SOURCES = asn1_decode_simple_der_fuzzer.c $(MAIN)
asn1_decode_simple_ber_fuzzer_SOURCES = asn1_decode_simple_ber_fuzzer.c $(MAIN)

# comment out below to exclude directories/files from the tarball
dist-hook:
	find $(srcdir) -name '*.options' -exec cp -v '{}' $(distdir) ';'
	find $(srcdir) -name '*.dict' -exec cp -v '{}' $(distdir) ';'
#	find $(srcdir) -name '*.in' -exec cp -vr '{}' $(distdir) ';'
	find $(srcdir) -name '*.repro' -exec cp -vr '{}' $(distdir) ';'

clean-local: code-coverage-clean
	rm -rf *.gc?? *.log lcov

distclean-local: code-coverage-dist-clean

oss-fuzz:
	if test "$$OUT" != ""; then \
	  XLIBS=""; \
	  for ccfile in *_fuzzer.c; do \
	    fuzzer=$$(basename $$ccfile .c); \
	    $$CXX $$CXXFLAGS -I$(top_builddir)/lib/includes -I$(top_srcdir)/lib/includes -I$(top_srcdir) \
	      "$${fuzzer}.c" -o "$${fuzzer}" \
	      ../lib/.libs/libtasn1.a $${LIB_FUZZING_ENGINE} \
	      -Wl,-Bstatic $${XLIBS} -Wl,-Bdynamic; \
	  done; \
	fi

.PHONY: oss-fuzz

LOG_COMPILER = $(VALGRIND)
