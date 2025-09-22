## Process this file with automake to produce Makefile.in

EXTRA_DIST = NEWS README

SUBDIRS = lib

if ENABLE_SUBIDS
SUBDIRS += libsubid
endif

SUBDIRS += src po contrib doc etc tests/unit

if ENABLE_REGENERATE_MAN
SUBDIRS += man
endif

CLEANFILES = man/8.out man/po/remove-potcdate.* man/*/login.defs.d man/*/*.mo
EXTRA_DIST = tests/
