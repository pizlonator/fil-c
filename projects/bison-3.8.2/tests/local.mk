## Makefile for Bison testsuite.

## Copyright (C) 2000-2015, 2018-2021 Free Software Foundation, Inc.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.

EXTRA_DIST +=					\
  %D%/README.md %D%/linear			\
  $(TESTSUITE_AT) %D%/testsuite %D%/testsuite.h

DISTCLEANFILES       += %D%/atconfig $(check_SCRIPTS)
MAINTAINERCLEANFILES += $(TESTSUITE)

## ------------ ##
## package.m4.  ##
## ------------ ##

$(top_srcdir)/%D%/package.m4: $(top_srcdir)/configure
	$(AM_V_GEN)rm -f $@ $@.tmp
	$(AM_V_at){ \
	  echo '# Signature of the current package.'; \
	  echo 'm4_define([AT_PACKAGE_NAME],      [$(PACKAGE_NAME)])'; \
	  echo 'm4_define([AT_PACKAGE_TARNAME],   [$(PACKAGE_TARNAME)])'; \
	  echo 'm4_define([AT_PACKAGE_VERSION],   [$(PACKAGE_VERSION)])'; \
	  echo 'm4_define([AT_PACKAGE_STRING],    [$(PACKAGE_STRING)])'; \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT], [$(PACKAGE_BUGREPORT)])'; \
	} >$@.tmp
	$(AM_V_at)mv $@.tmp $@


# Update the test cases.  Consider the latest test results to be the
# correct expectations, and change the test cases to match them.
.PHONY: update-tests
update-tests:
	$(AM_V_GEN)cd $(top_srcdir) \
	  && build-aux/update-test $(abs_builddir)/%D%/testsuite.dir/*/testsuite.log

## ------------------------- ##
## Generate the test suite.  ##
## ------------------------- ##

TESTSUITE_AT =                                \
  %D%/testsuite.at                            \
                                              \
  %D%/actions.at                              \
  %D%/c++.at                                  \
  %D%/calc.at                                 \
  %D%/conflicts.at                            \
  %D%/counterexample.at                       \
  %D%/cxx-type.at                             \
  %D%/d.at                                    \
  %D%/diagnostics.at                          \
  %D%/existing.at                             \
  %D%/glr-regression.at                       \
  %D%/headers.at                              \
  %D%/input.at                                \
  %D%/java.at                                 \
  %D%/javapush.at                             \
  %D%/local.at                                \
  %D%/m4.at                                   \
  %D%/named-refs.at                           \
  %D%/output.at                               \
  %D%/package.m4                              \
  %D%/push.at                                 \
  %D%/reduce.at                               \
  %D%/regression.at                           \
  %D%/report.at                               \
  %D%/scanner.at                              \
  %D%/sets.at                                 \
  %D%/skeletons.at                            \
  %D%/synclines.at                            \
  %D%/torture.at                              \
  %D%/types.at

TESTSUITE = $(top_srcdir)/%D%/testsuite

AUTOTEST = $(AUTOM4TE) --language=autotest
AUTOTESTFLAGS = -I $(top_srcdir)/%D%
$(TESTSUITE): $(TESTSUITE_AT)
	$(AM_V_GEN) \
	  $(AUTOTEST) $(AUTOTESTFLAGS) $(srcdir)/%D%/testsuite.at -o $@.tmp
	$(AM_V_at)"$(PERL)" -pi -e 's/\@tb\@/\t/g' $@.tmp
	$(AM_V_at)mv $@.tmp $@


## -------------------- ##
## Run the test suite.  ##
## -------------------- ##

# Move into %D%/ so that testsuite.dir etc. be created there.
RUN_TESTSUITE = $(SHELL) $(TESTSUITE) -C %D% $(TESTSUITEFLAGS)
check_SCRIPTS += $(BISON) %D%/atconfig %D%/atlocal
RUN_TESTSUITE_deps = all $(TESTSUITE) $(check_SCRIPTS)

clean-local: clean-local-tests
clean-local-tests:
	test ! -f $(TESTSUITE) || $(TESTSUITE) -C %D% --clean

.PHONY: recheck
recheck: $(RUN_TESTSUITE_deps)
	$(RUN_TESTSUITE)					\
	  $$(perl -n						\
	     -e 'eof && /^(\d+).*: FAILED/ && print "$$1 "'	\
		%D%/testsuite.dir/*/testsuite.log)

check-local: check-tests

.PHONY: check-tests
check-tests: $(RUN_TESTSUITE_deps)
	$(RUN_TESTSUITE)

# Run the test suite on the *installed* tree.
installcheck-local: $(RUN_TESTSUITE_deps)
	$(RUN_TESTSUITE) AUTOTEST_PATH='$(bindir)'

# Be real mean with it.
.PHONY: maintainer-check-g++
maintainer-check-g++: $(RUN_TESTSUITE_deps)
	$(RUN_TESTSUITE) --compile-c-with-cxx

.PHONY: maintainer-check-posix
maintainer-check-posix: $(RUN_TESTSUITE_deps)
	$(RUN_TESTSUITE) POSIXLY_CORRECT=1 _POSIX2_VERSION=200112

.PHONY: maintainer-check-valgrind
VALGRIND_OPTS = --leak-check=full --show-reachable=yes --gen-suppressions=all \
  $(VALGRIND_OPTS_SUPPRESSION)
maintainer-check-valgrind: $(RUN_TESTSUITE_deps)
	test 'x$(VALGRIND)' = x ||					\
	  $(RUN_TESTSUITE)						\
	    PREBISON='$(VALGRIND) -q' PREPARSER='$(VALGRIND) -q'	\
	    VALGRIND_OPTS="$(VALGRIND_OPTS)"

.PHONY: maintainer-check
maintainer-check: maintainer-check-posix maintainer-check-valgrind maintainer-check-g++

.PHONY: maintainer-check-push
maintainer-check-push:
	$(MAKE) $(AM_MAKEFLAGS) maintainer-check			\
	  TESTSUITEFLAGS='BISON_USE_PUSH_FOR_PULL=1 $(TESTSUITEFLAGS)'

.PHONY: maintainer-check-xml
maintainer-check-xml:
	$(MAKE) $(AM_MAKEFLAGS) maintainer-check		\
	  TESTSUITEFLAGS='BISON_TEST_XML=1 $(TESTSUITEFLAGS)'

.PHONY: maintainer-check-release
maintainer-check-release: maintainer-check maintainer-check-push maintainer-check-xml
