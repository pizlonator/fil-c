## Process this file with automake to produce Makefile.in

TESTS = check_money
check_PROGRAMS = check_money
check_money_SOURCES = check_money.c $(top_builddir)/src/money.h
check_money_CFLAGS = @CHECK_CFLAGS@
check_money_LDADD = $(top_builddir)/src/libmoney.la @CHECK_LIBS@
