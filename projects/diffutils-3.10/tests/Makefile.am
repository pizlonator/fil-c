# tests for GNU diff

TESTS = \
  basic \
  bignum \
  binary \
  brief-vs-stat-zero-kernel-lies \
  cmp \
  colliding-file-names \
  diff3 \
  excess-slash \
  help-version	\
  ifdef \
  invalid-re	\
  function-line-vs-leading-space \
  ignore-matching-lines \
  label-vs-func	\
  large-subopt \
  new-file \
  no-dereference \
  no-newline-at-eof \
  stdin \
  strcoll-0-names \
  filename-quoting \
  strip-trailing-cr \
  timezone \
  colors \
  y2038-vs-32bit

XFAIL_TESTS = large-subopt

EXTRA_DIST = \
  $(TESTS) init.cfg init.sh t-local.sh envvar-check \
  large-subopt.in1 \
  large-subopt.in2

# Note that the first lines are statements.  They ensure that environment
# variables that can perturb tests are unset or set to expected values.
# The rest are envvar settings that propagate build-related Makefile
# variables to test scripts.
TESTS_ENVIRONMENT =					\
  tmp__=$${TMPDIR-/tmp};				\
  test -d "$$tmp__" && test -w "$$tmp__" || tmp__=.;	\
  . $(srcdir)/envvar-check;				\
  TMPDIR=$$tmp__; export TMPDIR;			\
							\
  if test -n "$$BASH_VERSION" || (eval "export v=x") 2>/dev/null; then \
    export_with_values () { export "$$@"; };		\
  else							\
    export_with_values ()				\
    {							\
      sed_extract_var='s/=.*//';			\
      sed_quote_value="s/'/'\\\\''/g;s/=\\(.*\\)/='\\1'/";\
      for arg in "$$@"; do				\
        var=`echo "$$arg" | sed "$$sed_extract_var"`;	\
        arg=`echo "$$arg" | sed "$$sed_quote_value"`;	\
        eval "$$arg";					\
        export "$$var";					\
      done;						\
    };							\
  fi;							\
							\
  export_with_values					\
  VERSION='$(VERSION)'					\
  LOCALE_FR='$(LOCALE_FR)'				\
  LOCALE_FR_UTF8='$(LOCALE_FR_UTF8)'			\
  AWK=$(AWK)						\
  GREP_OPTIONS=''					\
  LC_ALL=C						\
  abs_top_builddir='$(abs_top_builddir)'		\
  abs_top_srcdir='$(abs_top_srcdir)'			\
  abs_srcdir='$(abs_srcdir)'				\
  built_programs="`$(built_programs)`"			\
  srcdir='$(srcdir)'					\
  top_srcdir='$(top_srcdir)'				\
  CC='$(CC)'						\
  DIFFUTILS_TEST_NAME=`echo $$tst|sed 's,^\./,,;s,/,-,g'` \
  MAKE=$(MAKE)						\
  MALLOC_PERTURB_=$(MALLOC_PERTURB_)			\
  PACKAGE_BUGREPORT='$(PACKAGE_BUGREPORT)'		\
  PACKAGE_VERSION=$(PACKAGE_VERSION)			\
  PERL='$(PERL)'					\
  SHELL='$(SHELL)'					\
  PREFERABLY_POSIX_SHELL='$(PREFERABLY_POSIX_SHELL)'	\
  REPLACE_GETCWD=$(REPLACE_GETCWD)			\
  PATH='$(abs_top_builddir)/src$(PATH_SEPARATOR)'"$$PATH"	\
  ; 9>&2

LOG_COMPILER= $(SHELL)

built_programs =							\
  echo 'spy:;@echo $$(PROGRAMS)'					\
    | { (cd ../src && MAKEFLAGS= $(MAKE) -s -f Makefile -f - spy)	\
          | tr ' ' '\n'							\
          | sed '/^$$/d; s,$(EXEEXT)$$,,'				\
          | sort -u							\
          | tr '\n' ' '; echo; }					\
    | sed 's/ $$//'

VERBOSE = yes
