## Process this file with automake to produce Makefile.in

TESTS = \
	check_check_export	\
	check_check		\
	test_output.sh		\
	test_check_nofork.sh    \
	test_check_nofork_teardown.sh    \
	test_xml_output.sh	\
	test_log_output.sh      \
	test_set_max_msg_size.sh	\
	test_tap_output.sh

# check_thread_stress is kind of slow.
# add this line back to TESTS to enable check_thread_stress
# 	check_thread_stress     \
#
# uncomment if check_thread_stress enabled
# XFAIL_TESTS = \
# 	check_thread_stress

noinst_PROGRAMS =		\
	check_check_export	\
	check_check		\
	check_stress		\
	check_thread_stress	\
	check_nofork		\
	check_nofork_teardown \
	check_mem_leaks		\
	check_set_max_msg_size	\
	ex_output

EXTRA_DIST = test_output.sh test_check_nofork.sh test_check_nofork_teardown.sh test_log_output.sh test_vars.in test_xml_output.sh test_tap_output.sh test_mem_leaks.sh test_output_strings test_set_max_msg_size.sh

if NO_TIMEOUT_TESTS
check_check_CFLAGS = -DTIMEOUT_TESTS_ENABLED=0
check_check_export_CFLAGS = -DTIMEOUT_TESTS_ENABLED=0
endif

check_check_export_SOURCES = \
	check_check.h		\
	check_check_sub.c	\
	check_check_master.c	\
	check_check_log.c	\
	check_check_fork.c	\
	check_check_tags.c      \
	check_check_export_main.c
check_check_export_LDADD = $(top_builddir)/src/libcheck.la $(top_builddir)/lib/libcompat.la

check_check_SOURCES = \
	check_check.h			\
	check_list.c			\
	check_check_sub.c		\
	check_check_master.c		\
	check_check_msg.c		\
	check_check_log.c		\
	check_check_log_internal.c	\
	check_check_limit.c		\
	check_check_fork.c		\
	check_check_fixture.c		\
	check_check_pack.c		\
	check_check_exit.c		\
	check_check_selective.c         \
	check_check_tags.c         	\
	check_check_main.c
check_check_LDADD = $(top_builddir)/src/libcheckinternal.la $(top_builddir)/lib/libcompat.la

check_mem_leaks_SOURCES = 	\
	check_mem_leaks.c 		\
	check_check_log.c		\
	check_check_fork.c		\
	check_check_exit.c		\
	check_check_selective.c	\
	check_check_tags.c         	\
	check_check_sub.c		\
	check_check_master.c
check_mem_leaks_LDADD = $(top_builddir)/src/libcheck.la $(top_builddir)/lib/libcompat.la
check_mem_leaks_CFLAGS = -DTIMEOUT_TESTS_ENABLED=0 -DMEMORY_LEAKING_TESTS_ENABLED=0

check_set_max_msg_size_SOURCES = 	\
	check_set_max_msg_size.c
check_set_max_msg_size_LDADD = $(top_builddir)/src/libcheck.la $(top_builddir)/lib/libcompat.la

check_stress_SOURCES = check_stress.c
check_stress_LDADD = $(top_builddir)/src/libcheck.la $(top_builddir)/lib/libcompat.la

check_thread_stress_SOURCES = check_thread_stress.c
check_thread_stress_LDADD = $(top_builddir)/src/libcheck.la $(top_builddir)/lib/libcompat.la @PTHREAD_LIBS@
check_thread_stress_CFLAGS = @PTHREAD_CFLAGS@

check_nofork_SOURCES = check_nofork.c
check_nofork_LDADD = $(top_builddir)/src/libcheck.la $(top_builddir)/lib/libcompat.la

check_nofork_teardown_SOURCES = check_nofork_teardown.c
check_nofork_teardown_LDADD = $(top_builddir)/src/libcheck.la $(top_builddir)/lib/libcompat.la

ex_output_SOURCES = ex_output.c
ex_output_LDADD = $(top_builddir)/src/libcheck.la $(top_builddir)/lib/libcompat.la

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src

CLEANFILES = *~ *.log *.xml *.tap test_logfile
