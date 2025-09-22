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

if ENABLE_GTK_DOC
SUBDIRS = reference
endif

EXTRA_DIST = gdoc TODO stamp_docs COPYING COPYING.LESSER

info_TEXINFOS = libtasn1.texi
libtasn1_TEXINFOS = fdl-1.3.texi $(gdoc_TEXINFOS)
libtasn1_TEXINFOS += asn1Coding-help.texi asn1Decoding-help.texi asn1Parser-help.texi

AM_MAKEINFOHTMLFLAGS = --no-split $(AM_MAKEINFOFLAGS)

dist_man_MANS = $(gdoc_MANS) asn1Parser.1 asn1Coding.1 asn1Decoding.1

HELP2MAN_OPTS = --info-page libtasn1

asn1Coding.1: $(top_srcdir)/src/asn1Coding.c $(top_srcdir)/.version
	$(AM_V_GEN)$(HELP2MAN) $(HELP2MAN_OPTS) \
		--name="ASN.1 DER encoder" \
		--output=$@ $(top_builddir)/src/asn1Coding$(EXEEXT)

asn1Decoding.1: $(top_srcdir)/src/asn1Decoding.c $(top_srcdir)/.version
	$(AM_V_GEN)$(HELP2MAN) $(HELP2MAN_OPTS) \
		--name="ASN.1 DER decoder" \
		--output=$@ $(top_builddir)/src/asn1Decoding$(EXEEXT)

asn1Parser.1: $(top_srcdir)/src/asn1Parser.c $(top_srcdir)/.version
	$(AM_V_GEN)$(HELP2MAN) $(HELP2MAN_OPTS) \
		--name="ASN.1 syntax tree generator for libtasn1" \
		--output=$@ $(top_builddir)/src/asn1Parser$(EXEEXT)

asn1Coding-help.texi: $(top_srcdir)/src/asn1Coding.c
	$(AM_V_GEN)$(top_builddir)/src/asn1Coding$(EXEEXT) --help > tmp-$@ && \
		mv tmp-$@ asn1Coding-help.texi

asn1Decoding-help.texi: $(top_srcdir)/src/asn1Decoding.c
	$(AM_V_GEN)$(top_builddir)/src/asn1Decoding$(EXEEXT) --help > tmp-$@ && \
		mv tmp-$@ asn1Decoding-help.texi

asn1Parser-help.texi: $(top_srcdir)/src/asn1Parser.c
	$(AM_V_GEN)$(top_builddir)/src/asn1Parser$(EXEEXT) --help > tmp-$@ && \
		mv tmp-$@ asn1Parser-help.texi

gdoc_MANS =
gdoc_MANS += man/asn1_get_length_der.3
gdoc_MANS += man/asn1_get_tag_der.3
gdoc_MANS += man/asn1_get_length_ber.3
gdoc_MANS += man/asn1_get_octet_der.3
gdoc_MANS += man/asn1_get_object_id_der.3
gdoc_MANS += man/asn1_get_bit_der.3
gdoc_MANS += man/asn1_der_decoding2.3
gdoc_MANS += man/asn1_der_decoding.3
gdoc_MANS += man/asn1_der_decoding_element.3
gdoc_MANS += man/asn1_der_decoding_startEnd.3
gdoc_MANS += man/asn1_expand_any_defined_by.3
gdoc_MANS += man/asn1_expand_octet_string.3
gdoc_MANS += man/asn1_decode_simple_der.3
gdoc_MANS += man/asn1_decode_simple_ber.3
gdoc_MANS += man/asn1_perror.3
gdoc_MANS += man/asn1_strerror.3
gdoc_MANS += man/asn1_find_node.3
gdoc_MANS += man/asn1_length_der.3
gdoc_MANS += man/asn1_octet_der.3
gdoc_MANS += man/asn1_encode_simple_der.3
gdoc_MANS += man/asn1_object_id_der.3
gdoc_MANS += man/asn1_bit_der.3
gdoc_MANS += man/asn1_der_coding.3
gdoc_MANS += man/asn1_write_value.3
gdoc_MANS += man/asn1_read_value.3
gdoc_MANS += man/asn1_read_value_type.3
gdoc_MANS += man/asn1_read_tag.3
gdoc_MANS += man/asn1_read_node_value.3
gdoc_MANS += man/asn1_array2tree.3
gdoc_MANS += man/asn1_delete_structure.3
gdoc_MANS += man/asn1_delete_structure2.3
gdoc_MANS += man/asn1_delete_element.3
gdoc_MANS += man/asn1_create_element.3
gdoc_MANS += man/asn1_print_structure.3
gdoc_MANS += man/asn1_number_of_elements.3
gdoc_MANS += man/asn1_find_structure_from_oid.3
gdoc_MANS += man/asn1_copy_node.3
gdoc_MANS += man/asn1_dup_node.3
gdoc_MANS += man/asn1_parser2tree.3
gdoc_MANS += man/asn1_parser2array.3
gdoc_MANS += man/asn1_check_version.3
gdoc_TEXINFOS =
gdoc_TEXINFOS += texi/decoding.c.texi
gdoc_TEXINFOS += texi/errors.c.texi
gdoc_TEXINFOS += texi/parser_aux.c.texi
gdoc_TEXINFOS += texi/coding.c.texi
gdoc_TEXINFOS += texi/element.c.texi
gdoc_TEXINFOS += texi/gstr.c.texi
gdoc_TEXINFOS += texi/structure.c.texi
gdoc_TEXINFOS += texi/ASN1.c.texi
gdoc_TEXINFOS += texi/version.c.texi
gdoc_TEXINFOS += texi/asn1_get_length_der.texi
gdoc_TEXINFOS += texi/asn1_get_tag_der.texi
gdoc_TEXINFOS += texi/asn1_get_length_ber.texi
gdoc_TEXINFOS += texi/asn1_get_octet_der.texi
gdoc_TEXINFOS += texi/asn1_get_object_id_der.texi
gdoc_TEXINFOS += texi/asn1_get_bit_der.texi
gdoc_TEXINFOS += texi/asn1_der_decoding2.texi
gdoc_TEXINFOS += texi/asn1_der_decoding.texi
gdoc_TEXINFOS += texi/asn1_der_decoding_element.texi
gdoc_TEXINFOS += texi/asn1_der_decoding_startEnd.texi
gdoc_TEXINFOS += texi/asn1_expand_any_defined_by.texi
gdoc_TEXINFOS += texi/asn1_expand_octet_string.texi
gdoc_TEXINFOS += texi/asn1_decode_simple_der.texi
gdoc_TEXINFOS += texi/asn1_decode_simple_ber.texi
gdoc_TEXINFOS += texi/asn1_perror.texi
gdoc_TEXINFOS += texi/asn1_strerror.texi
gdoc_TEXINFOS += texi/asn1_find_node.texi
gdoc_TEXINFOS += texi/asn1_length_der.texi
gdoc_TEXINFOS += texi/asn1_octet_der.texi
gdoc_TEXINFOS += texi/asn1_encode_simple_der.texi
gdoc_TEXINFOS += texi/asn1_object_id_der.texi
gdoc_TEXINFOS += texi/asn1_bit_der.texi
gdoc_TEXINFOS += texi/asn1_der_coding.texi
gdoc_TEXINFOS += texi/asn1_write_value.texi
gdoc_TEXINFOS += texi/asn1_read_value.texi
gdoc_TEXINFOS += texi/asn1_read_value_type.texi
gdoc_TEXINFOS += texi/asn1_read_tag.texi
gdoc_TEXINFOS += texi/asn1_read_node_value.texi
gdoc_TEXINFOS += texi/asn1_array2tree.texi
gdoc_TEXINFOS += texi/asn1_delete_structure.texi
gdoc_TEXINFOS += texi/asn1_delete_structure2.texi
gdoc_TEXINFOS += texi/asn1_delete_element.texi
gdoc_TEXINFOS += texi/asn1_create_element.texi
gdoc_TEXINFOS += texi/asn1_print_structure.texi
gdoc_TEXINFOS += texi/asn1_number_of_elements.texi
gdoc_TEXINFOS += texi/asn1_find_structure_from_oid.texi
gdoc_TEXINFOS += texi/asn1_copy_node.texi
gdoc_TEXINFOS += texi/asn1_dup_node.texi
gdoc_TEXINFOS += texi/asn1_parser2tree.texi
gdoc_TEXINFOS += texi/asn1_parser2array.texi
gdoc_TEXINFOS += texi/asn1_check_version.texi

GDOC_SRC = $(top_srcdir)/lib/decoding.c $(top_srcdir)/lib/errors.c	\
	$(top_srcdir)/lib/parser_aux.c $(top_srcdir)/lib/coding.c	\
	$(top_srcdir)/lib/element.c $(top_srcdir)/lib/gstr.c		\
	$(top_srcdir)/lib/structure.c $(top_builddir)/lib/ASN1.c		\
	$(top_srcdir)/lib/version.c

compare-makefile:
	@MANS=""; \
	TEXINFOS=""; \
	for i in $(GDOC_SRC); do \
		BASE=`basename $$i`; \
		TEXINFOS="$$TEXINFOS\ngdoc_TEXINFOS += texi/$$BASE.texi"; \
	done; \
	FUNCS=`$(srcdir)/gdoc -listfunc $(GDOC_SRC)`; \
	for i in $$FUNCS; do \
		MANS="$$MANS\ngdoc_MANS += man/$$i.3"; \
		TEXINFOS="$$TEXINFOS\ngdoc_TEXINFOS += texi/$$i.texi"; \
	done; \
	grep -v -e '^gdoc_MANS += ' -e '^gdoc_TEXINFOS += ' $(srcdir)/Makefile.am | \
		perl -p -e "s,^gdoc_MANS =,gdoc_MANS =$$MANS,;" | \
		perl -p -e "s,^gdoc_TEXINFOS =,gdoc_TEXINFOS =$$TEXINFOS,;" \
		> tmp-$@;
	diff -u $(srcdir)/Makefile.am tmp-$@
	rm -f tmp-$@

$(gdoc_MANS) $(gdoc_TEXINFOS): stamp_docs

clean-local:
	-rm -f stamp_docs

stamp_docs: $(top_srcdir)/.version
	$(mkdir_p) man texi; \
	for i in `$(srcdir)/gdoc -listfunc $(GDOC_SRC)`; do \
		echo -n "Creating documentation for $$i... " && \
		$(srcdir)/gdoc -man \
			-module $(PACKAGE) \
			-pkg-name "$(PACKAGE_NAME)" \
			-sourceversion $(VERSION) \
			-include libtasn1.h \
			-seeinfo $(PACKAGE) -verbatimcopying \
			-copyright "2006-2022 Free Software Foundation, Inc." \
			-function $$i \
			$(GDOC_SRC) > man/$$i.3 && \
		$(srcdir)/gdoc -texinfo -function $$i \
			$(GDOC_SRC) > texi/$$i.texi && \
		echo "ok"; \
	done; \
	for i in $(GDOC_SRC); do \
		BASE=`basename $$i`; \
		echo -n "Creating documentation for $$i... " && \
		$(srcdir)/gdoc -texinfo $$i > texi/$$BASE.texi && \
		echo "ok"; \
	done
	touch $@

.PHONY: compare-makefile
