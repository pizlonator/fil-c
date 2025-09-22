## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES		= libcheck.la
noinst_LTLIBRARIES	= libcheckinternal.la

include_HEADERS		= check.h

EXTRA_DIST		= check.h.in

AM_CFLAGS		= @GCOV_CFLAGS@ @PTHREAD_CFLAGS@ $(LIBSUBUNIT_CFLAGS)

CFILES =\
	check.c		\
	check_error.c	\
	check_list.c	\
	check_log.c	\
	check_msg.c	\
	check_pack.c	\
	check_print.c	\
	check_run.c	\
	check_str.c

HFILES =\
	check.h		\
	check_error.h	\
	check_impl.h	\
	check_list.h	\
	check_log.h	\
	check_msg.h	\
	check_pack.h	\
	check_print.h	\
	check_str.h


EXPORT_SYM	= exported.sym
$(EXPORT_SYM): check.h.in
	${SED} -n -e 's/^..*CK_EXPORT[[:space:]][[:space:]]*\([[:alnum:]_][[:alnum:]_]*\)..*$$/\1/p' @top_srcdir@/src/check.h.in > $@

libcheck_la_DEPENDENCIES= $(EXPORT_SYM)
libcheck_la_LDFLAGS	= -no-undefined -export-symbols $(EXPORT_SYM)
libcheck_la_SOURCES	= $(CFILES) $(HFILES)
libcheck_la_LIBADD	= @GCOV_LIBS@ @PTHREAD_LIBS@ $(LIBSUBUNIT_LIBS) $(top_builddir)/lib/libcompat.la

libcheckinternal_la_LDFLAGS     = -no-undefined
libcheckinternal_la_SOURCES	= $(CFILES) $(HFILES)
libcheckinternal_la_LIBADD	= @GCOV_LIBS@ @PTHREAD_LIBS@ $(LIBSUBUNIT_LIBS) $(top_builddir)/lib/libcompat.la

CLEANFILES	= *~ *.gcno $(EXPORT_SYM)

LCOV_INPUT	= $(CFILES:%.c=.libs/%.gcda)
LCOV_OUTPUT	= lcov.info
LCOV_HTML	= lcov_html
LCOV_LCOV	= @LCOV@
LCOV_GENHTML	= @GENHTML@

lcov: $(LCOV_HTML)

$(LCOV_INPUT): libcheck.la libcheckinternal.la
	@$(MAKE) -C $(top_builddir)/tests check

$(LCOV_OUTPUT): $(LCOV_INPUT)
	$(LCOV_LCOV) --capture --directory . --base-directory . --output-file $@

$(LCOV_HTML): $(LCOV_OUTPUT)
	-$(RM) -r $@
	LANG=C $(LCOV_GENHTML) --output-directory $@ --title "Check Code Coverage" --show-details $<
	@echo "Point a web browser at $(LCOV_HTML)/index.html to see results."

clean-local: lcov-clean

.PHONY: lcov-clean
lcov-clean:
	-$(RM) -r $(LCOV_HTML) $(LCOV_OUTPUT)
