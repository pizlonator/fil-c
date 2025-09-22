## Copyright (C) 2001-2015, 2018-2021 Free Software Foundation, Inc.
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

CLEANDIRS += %D%/*.dSYM

bin_PROGRAMS = src/bison
# Prettify Automake-computed names of compiled objects.
src_bison_SHORTNAME = bison

src_bison_CPPFLAGS = $(AM_CPPFLAGS) -DINSTALLDIR=\"$(bindir)\"
if RELOCATABLE_VIA_LD
  src_bison_LDFLAGS = `$(RELOCATABLE_LDFLAGS) $(bindir)`
endif

src_bison_CFLAGS = $(AM_CFLAGS) $(WERROR_CFLAGS)
src_bison_SOURCES =                             \
  src/AnnotationList.c                          \
  src/AnnotationList.h                          \
  src/InadequacyList.c                          \
  src/InadequacyList.h                          \
  src/Sbitset.c                                 \
  src/Sbitset.h                                 \
  src/assoc.c                                   \
  src/assoc.h                                   \
  src/closure.c                                 \
  src/closure.h                                 \
  src/complain.c                                \
  src/complain.h                                \
  src/conflicts.c                               \
  src/conflicts.h                               \
  src/counterexample.c                          \
  src/counterexample.h                          \
  src/derivation.c                              \
  src/derivation.h                              \
  src/derives.c                                 \
  src/derives.h                                 \
  src/files.c                                   \
  src/files.h                                   \
  src/fixits.c                                  \
  src/fixits.h                                  \
  src/flex-scanner.h                            \
  src/getargs.c                                 \
  src/getargs.h                                 \
  src/glyphs.c                                  \
  src/glyphs.h                                  \
  src/gram.c                                    \
  src/gram.h                                    \
  src/graphviz.c                                \
  src/graphviz.h                                \
  src/ielr.c                                    \
  src/ielr.h                                    \
  src/lalr.c                                    \
  src/lalr.h                                    \
  src/location.c                                \
  src/location.h                                \
  src/lr0.c                                     \
  src/lr0.h                                     \
  src/lssi.c                                    \
  src/lssi.h                                    \
  src/main.c                                    \
  src/muscle-tab.c                              \
  src/muscle-tab.h                              \
  src/named-ref.c                               \
  src/named-ref.h                               \
  src/nullable.c                                \
  src/nullable.h                                \
  src/output.c                                  \
  src/output.h                                  \
  src/parse-gram.y                              \
  src/parse-simulation.c                        \
  src/parse-simulation.h                        \
  src/print-graph.c                             \
  src/print-graph.h                             \
  src/print-xml.c                               \
  src/print-xml.h                               \
  src/print.c                                   \
  src/print.h                                   \
  src/reader.c                                  \
  src/reader.h                                  \
  src/reduce.c                                  \
  src/reduce.h                                  \
  src/relation.c                                \
  src/relation.h                                \
  src/scan-code-c.c                             \
  src/scan-code.h                               \
  src/scan-gram-c.c                             \
  src/scan-gram.h                               \
  src/scan-skel-c.c                             \
  src/scan-skel.h                               \
  src/state.c                                   \
  src/state.h                                   \
  src/state-item.c                              \
  src/state-item.h                              \
  src/strversion.c                              \
  src/strversion.h                              \
  src/symlist.c                                 \
  src/symlist.h                                 \
  src/symtab.c                                  \
  src/symtab.h                                  \
  src/system.h                                  \
  src/tables.c                                  \
  src/tables.h                                  \
  src/uniqstr.c                                 \
  src/uniqstr.h

EXTRA_src_bison_SOURCES =                       \
  src/scan-code.l                               \
  src/scan-gram.l                               \
  src/scan-skel.l

BUILT_SOURCES +=                                \
  src/parse-gram.c                              \
  src/parse-gram.h                              \
  src/scan-code.c                               \
  src/scan-gram.c                               \
  src/scan-skel.c

# Although conceptually most of these guys would make more sense in the
# definition of libbison, beware that they might expand as flags such as
# `-lm`.  Keep them here.  Or use a Libtool convenience library.
src_bison_LDADD =                               \
  lib/libbison.a                                \
  $(ISNAND_LIBM)                                \
  $(ISNANF_LIBM)                                \
  $(ISNANL_LIBM)                                \
  $(LDEXPL_LIBM)                                \
  $(LDEXP_LIBM)                                 \
  $(LIBTHREAD)                                  \
  $(LIB_CLOCK_GETTIME)                          \
  $(LIB_GETHRXTIME)                             \
  $(LIB_HARD_LOCALE)                            \
  $(LIB_MBRTOWC)                                \
  $(LIB_SETLOCALE_NULL)                         \
  $(LIBICONV)                                   \
  $(LIBINTL)                                    \
  $(LIBTEXTSTYLE)


EXTRA_DIST += %D%/i18n-strings.c


## ------ ##
## yacc.  ##
## ------ ##

if ENABLE_YACC
  nodist_bin_SCRIPTS = src/yacc
endif
EXTRA_SCRIPTS = src/yacc
MOSTLYCLEANFILES += src/yacc
