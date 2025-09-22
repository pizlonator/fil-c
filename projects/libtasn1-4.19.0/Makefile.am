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

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
AM_DISTCHECK_DVI_TARGET = html

EXTRA_DIST = windows/asn1-parser/asn1-parser.vcproj			\
	windows/asn1-parser/config.h windows/libtasn1.ncb		\
	windows/libtasn1.sln windows/libtasn1.suo			\
	windows/gnulib/getopt.h windows/gnulib/gnulib.vcproj		\
	windows/libtasn1/libtasn1.vcproj				\
	windows/test-parser/test-parser.vcproj				\
	windows/test-tree/test-tree.vcproj windows/libtasn14win.mk
EXTRA_DIST += cfg.mk maint.mk CONTRIBUTING.md README.md

SUBDIRS = lib src fuzz tests

if ENABLE_DOC
SUBDIRS += doc examples
endif

include $(top_srcdir)/aminclude_static.am

ACLOCAL_AMFLAGS = -I m4 -I src/gl/m4

ABIDW_COMMON = --no-show-locs --no-corpus-path
ABIGNORE_FILE = "$(top_srcdir)/devel/libtasn1.abignore"
LIBGNUTLS_ABI_LAST_FILE = "$(top_srcdir)/devel/libtasn1-latest-$$(uname -m).abi"

abi-dump-versioned: lib/libtasn1.la
	@echo "**************************************************"
	@echo "Generating versioned ABI files of current libtasn1"
	@echo "**************************************************"
	@abidw lib/.libs/libtasn1.so $(ABIDW_COMMON) --suppressions $(ABIGNORE_FILE) --out-file "$(srcdir)/devel/libtasn1-$(VERSION)-$$(uname -m).abi"

abi-dump-latest: lib/libtasn1.la
	@echo "****************************************"
	@echo "Generating ABI files of current libtasn1"
	@echo "****************************************"
	@abidw lib/.libs/libtasn1.so $(ABIDW_COMMON) --suppressions $(ABIGNORE_FILE) --out-file $(LIBGNUTLS_ABI_LAST_FILE)
	@rm -f "./devel/libtasn1-latest-$$(uname -m).tmp"

abi-check-latest: lib/libtasn1.la
	@echo "Checking whether the latest ABI dump matches"
	@abidiff --suppressions $(ABIGNORE_FILE) lib/.libs/libtasn1.so $(LIBGNUTLS_ABI_LAST_FILE) --hd2 "$(builddir)/lib/includes/"; if test $$? != 0;then \
		echo "*********************************************************"; \
		echo "libtasn1 ABI has changed; use 'make files-update'       "; \
		echo "and use 'git diff' to check correctness before committing"; \
		echo "*********************************************************"; \
		false; \
	fi
	@echo "********************************"
	@echo "Current release matches ABI dump"
	@echo "********************************"

ABICHECK_COMMON = --no-added-syms
abi-check: lib/libtasn1.la
	for file in $$(echo $(srcdir)/devel/libtasn1-*-$$(uname -m).abi);do \
		echo "Comparing libtasn1 with $$file"; \
		abidiff $${file} lib/.libs/libtasn1.so $(ABICHECK_COMMON) --suppressions $(ABIGNORE_FILE) --hd2 "$(builddir)/lib/includes//"; \
		if test $$? != 0;then \
			echo "****************************************************************************"; \
			echo "ABI check failed; If intentional add suppression in devel/libtasn1.abignore"; \
			echo "****************************************************************************"; \
			false; \
		fi; \
	done
	@echo "********************"
	@echo "ABI checks completed"
	@echo "********************"

files-update: abi-dump-latest

local-code-coverage-output: code-coverage-capture
	cat "GNU Libtasn1-$(VERSION)-coverage/index.html"|grep headerCovTableEntry|grep '%'|head -1|sed 's/^.*>\([0-9]\+\.[0-9]\+\s*%\)<.*$$/ coverage lines: \1/' || true

clean-local: code-coverage-clean
distclean-local: code-coverage-dist-clean

# git-version-gen
EXTRA_DIST += $(top_srcdir)/.version
BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook: gen-ChangeLog
	echo $(VERSION) > $(distdir)/.tarball-version
	if test -d "$(top_srcdir)/devel"; then \
		$(MAKE) -C $(top_builddir) abi-check-latest; \
		make -C doc/ compare-makefile; \
	fi

.PHONY: gen-ChangeLog
gen-ChangeLog:
	$(AM_V_GEN)if test -d .git; then				\
		$(top_srcdir)/build-aux/gitlog-to-changelog >		\
			$(distdir)/cl-t &&				\
		{ rm -f $(distdir)/ChangeLog &&				\
		  mv $(distdir)/cl-t $(distdir)/ChangeLog; }		\
	fi

.PHONY: abi-check abi-dump-versioned abi-dump-latest
