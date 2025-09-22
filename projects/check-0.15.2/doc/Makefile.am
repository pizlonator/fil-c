## Process this file with automake to produce Makefile.in

info_TEXINFOS = check.texi
check_TEXINFOS = fdl.texi

check_html: $(srcdir)/check.texi
	texi2html --output=check_html --top-file=index.html --split=chapter check.texi

doxygen:
	doxygen $(srcdir)/doxygen.conf

## if graphviz does not exist disable graph generation
if USE_GRAPHVIZ
doxygen-devel:
	doxygen $(srcdir)/doxygen-devel.conf
else
doxygen-devel:
	( cat $(srcdir)/doxygen-devel.conf; echo "HAVE_DOT = NO" ) | doxygen -
	@ >&2 echo "graphviz not found, graphs will not be rendered."
endif

## we need to include several diffs as we evolve the example in the
## tutorial.  this means we'll generate them from the example source.

$(srcdir)/check.texi: money.1-2.h.diff \
	    money.1-3.c.diff \
	    money.3-4.c.diff \
	    money.4-5.c.diff \
	    money.5-6.c.diff \
	    check_money.1-2.c.diff \
	    check_money.2-3.c.diff \
	    check_money.3-6.c.diff \
	    check_money.6-7.c.diff

eg_root = $(top_srcdir)/doc/example
eg_src = $(eg_root)/src
eg_tests = $(eg_root)/tests

# If the filterdiff tool is available, use it to filter timestamps
# from diff files. Otherwise, the timestamps change between builds
# and the built output will not be reproducible
if USE_FILTERDIFF
    filter_timestapms = | filterdiff --remove-timestamps
else
    filter_timestapms =
endif

## now a rule for each diff.  the redundancy here can probably be
## parameterized, but I don't know how.  if you know, please tell us!

# diff returns 1 if there is a difference, but we don't want make to
# think that means there is an error
money.1-2.h.diff: $(eg_src)/money.1.h $(eg_src)/money.2.h
	cd $(eg_root); \
	diff -U 100 src/money.1.h src/money.2.h ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

money.1-3.c.diff: $(eg_src)/money.1.c $(eg_src)/money.3.c
	cd $(eg_root); \
	diff -U 100 src/money.1.c src/money.3.c ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

money.3-4.c.diff: $(eg_src)/money.3.c $(eg_src)/money.4.c
	cd $(eg_root); \
	diff -U 100 src/money.3.c src/money.4.c ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

money.4-5.c.diff: $(eg_src)/money.4.c $(eg_src)/money.5.c
	cd $(eg_root); \
	diff -U 100 src/money.4.c src/money.5.c ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

money.5-6.c.diff: $(eg_src)/money.5.c $(eg_src)/money.6.c
	cd $(eg_root); \
	diff -U 100 src/money.5.c src/money.6.c ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

check_money.1-2.c.diff: $(eg_tests)/check_money.1.c $(eg_tests)/check_money.2.c
	cd $(eg_root); \
	diff -U 100 tests/check_money.1.c tests/check_money.2.c ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

check_money.2-3.c.diff: $(eg_tests)/check_money.2.c $(eg_tests)/check_money.3.c
	cd $(eg_root); \
	diff -U 100 tests/check_money.2.c tests/check_money.3.c ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

check_money.3-6.c.diff: $(eg_tests)/check_money.3.c $(eg_tests)/check_money.6.c
	cd $(eg_root); \
	diff -U 100 tests/check_money.3.c tests/check_money.6.c ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

check_money.6-7.c.diff: $(eg_tests)/check_money.6.c $(eg_tests)/check_money.7.c
	cd $(eg_root); \
	diff -U 100 tests/check_money.6.c tests/check_money.7.c ${filter_timestapms} > @abs_builddir@/$@ || test $$? -eq 1; \
	cd -;

# explicitly list every file in the example.

example_docs = example/Makefile.am \
               example/README \
               example/configure.ac

example_src_docs = example/src/Makefile.am \
                   example/src/main.c \
                   example/src/money.c \
                   example/src/money.h \
		   example/src/money.1.h \
		   example/src/money.2.h \
		   example/src/money.1.c \
		   example/src/money.3.c \
		   example/src/money.4.c \
		   example/src/money.5.c \
		   example/src/money.6.c

example_tests_docs = example/tests/Makefile.am \
                     example/tests/check_money.c \
                     example/tests/check_money.1.c \
                     example/tests/check_money.2.c \
                     example/tests/check_money.3.c \
                     example/tests/check_money.6.c \
                     example/tests/check_money.7.c

example_cmake = example/CMakeLists.txt \
                example/src/CMakeLists.txt \
                example/tests/CMakeLists.txt \
                example/cmake/config.h.in \
                example/cmake/COPYING-CMAKE-SCRIPTS.txt \
                example/cmake/FindCheck.cmake

## what to clean

CLEANFILES = *~ *.diff

clean-local:
	rm -rf check_html
	rm -rf doxygen
## what to distribute

EXTRA_DIST = $(example_docs) \
             $(example_src_docs) \
             $(example_tests_docs) \
             $(example_cmake)

## what to install

docdir = $(datadir)/doc/$(PACKAGE)

# install money example

exampledir = $(docdir)/example
example_DATA = $(example_docs)

examplesrcdir = $(docdir)/example/src
examplesrc_DATA = $(example_src_docs)

exampletestsdir = $(docdir)/example/tests
exampletests_DATA = $(example_tests_docs)
