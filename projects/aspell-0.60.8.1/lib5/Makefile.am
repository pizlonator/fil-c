
if INCREMENTED_SONAME

lib_LTLIBRARIES = libaspell.la
libaspell_la_SOURCES = aspell-dummy.cpp
libaspell_la_LDFLAGS = -version-info 16:0:1
libaspell_la_LIBADD = ../libaspell.la

if PSPELL_COMPATIBILITY
lib_LTLIBRARIES += libpspell.la
libpspell_la_SOURCES = pspell-dummy.cpp
libpspell_la_LDFLAGS = -version-info 16:0:1
libpspell_la_LIBADD = ../libaspell.la
endif

endif
