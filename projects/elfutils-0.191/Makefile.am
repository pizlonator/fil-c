## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 1996-2006, 2008, 2009, 2015 Red Hat, Inc.
##
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
##
ACLOCAL_AMFLAGS = -I m4

# automake already tells which subdir is being entered.
# Don't make make repeat.
AM_MAKEFLAGS = --no-print-directory

pkginclude_HEADERS = version.h

SUBDIRS = config lib libelf libcpu backends libebl libdwelf libdwfl libdw \
	  libasm debuginfod src po doc tests

EXTRA_DIST = elfutils.spec GPG-KEY NOTES CONTRIBUTING SECURITY \
	     COPYING COPYING-GPLV2 COPYING-LGPLV3

# Make sure the test install uses lib64 when $LIB will yield lib64.
# Make sure the test build uses the same compiler, which on e.g. ppc64
# determines which platform we are actually testing.
# Run all tests under valgrind.
AM_DISTCHECK_CONFIGURE_FLAGS = \
	--libdir=`echo $(libdir) | sed "s,^$(exec_prefix),$$dc_install_base,"`\
	CC="$(CC)"

distcheck-hook:
	chmod -R u+w $(distdir)

rpm: dist
	rpmbuild -ts elfutils-@PACKAGE_VERSION@.tar.bz2

rpmbuild: dist
	mkdir -p rpmbuild
	rpmbuild -ta elfutils-@PACKAGE_VERSION@.tar.bz2 \
		--define "_topdir $(shell pwd)/rpmbuild"

if GCOV

COVERAGE_OUTPUT_FILE = $(PACKAGE_NAME).lcov
COVERAGE_OUTPUT_DIRECTORY = coverage
COVERAGE_OUTPUT_INDEX_HTML = $(COVERAGE_OUTPUT_DIRECTORY)/index.html
COVERAGE_TITLE = $(PACKAGE_NAME)-$(PACKAGE_VERSION)

COVERAGE_DIRS = $(filter-out tests,$(SUBDIRS))
src_COVERAGE_DIRS = $(patsubst %,$(srcdir)/%,$(COVERAGE_DIRS))
build_COVERAGE_DIRS = $(patsubst %,$(builddir)/%,$(COVERAGE_DIRS))
all_COVERAGE_DIRS = $(sort $(src_COVERAGE_DIRS) $(build_COVERAGE_DIRS))
LCOV_DIRS_ARGS = $(patsubst %,--directory=%,$(all_COVERAGE_DIRS))

CLEANFILES = $(COVERAGE_OUTPUT_FILE)

.PHONY: coverage coverage-clean

clean-local: coverage-clean
distclean-local: coverage-clean

coverage-clean:
	-rm -rf $(COVERAGE_OUTPUT_DIRECTORY)

coverage: $(COVERAGE_OUTPUT_INDEX_HTML)
	@echo 'file://$(abs_builddir)/$(COVERAGE_OUTPUT_INDEX_HTML)'

$(COVERAGE_OUTPUT_INDEX_HTML): $(COVERAGE_OUTPUT_FILE)
	LC_ALL=C $(GENHTML) \
		--legend \
		--show-details \
		--rc=genhtml_branch_coverage=1 \
		--title='$(COVERAGE_TITLE)' \
		--prefix='$(abspath $(abs_srcdir))' \
		--prefix='$(realpath $(abs_srcdir))' \
		--prefix='$(abspath $(abs_builddir)/..)' \
		--prefix='$(realpath $(abs_builddir)/..)' \
		--output-directory='$(COVERAGE_OUTPUT_DIRECTORY)' \
		$<

$(COVERAGE_OUTPUT_FILE):
	$(LCOV) \
		--capture \
		--no-external \
		--no-checksum \
		--rc=lcov_branch_coverage=1 \
		--gcov-tool='$(GCOV)' \
		--output-file='$@' \
		$(LCOV_DIRS_ARGS)

endif

# Tell version 3.79 and up of GNU make to not build goals in this
# directory in parallel.
.NOTPARALLEL:
