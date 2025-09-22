## Copyright (C) 2001-2003, 2005-2015, 2018-2021 Free Software
## Foundation, Inc.

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

AM_MAKEINFOFLAGS =                                              \
  --no-split                                                    \
  --set-customization-variable=SECTION_NAME_IN_TITLE=true       \
  --set-customization-variable=AVOID_MENU_REDUNDANCY=true       \
  --set-customization-variable=ICONS=true

info_TEXINFOS = %D%/bison.texi
%C%_bison_TEXINFOS =                            \
  $(CROSS_OPTIONS_TEXI)                         \
  %D%/fdl.texi                                  \
  %D%/gpl-3.0.texi                              \
  %D%/relocatable.texi

# Cannot express dependencies directly on file names because of Automake.
# Obfuscate with a variable.
%C%_bison = %D%/bison
$(%C%_bison).dvi: $(FIGS_GV:.gv=.eps)
$(%C%_bison).info: $(FIGS_GV:.gv=.txt)
$(%C%_bison).pdf: $(FIGS_GV:.gv=.pdf)
$(%C%_bison).html: $(FIGS_GV:.gv=.svg)

TEXI2DVI = texi2dvi --build-dir=%D%/bison.t2d -I %D%
CLEANDIRS += %D%/bison.t2d

MOSTLYCLEANFILES += $(top_srcdir)/%D%/*.tmp

CROSS_OPTIONS_PL = $(top_srcdir)/build-aux/cross-options.pl
CROSS_OPTIONS_TEXI = $(top_srcdir)/%D%/cross-options.texi
$(CROSS_OPTIONS_TEXI): %D%/bison.help $(CROSS_OPTIONS_PL)
# Create $@~ which is the previous contents.  Don't use 'mv' here so
# that even if we are interrupted, the file is still available for
# diff in the next run.  Note that $@ might not exist yet.
	$(AM_V_GEN){ test ! -f $@ || cat $@; } >$@~
	$(AM_V_at)test ! -f $@.tmp || rm -f $@.tmp
	$(AM_V_at)$(PERL) $(CROSS_OPTIONS_PL) $(top_srcdir)/src/scan-gram.l \
	  <$(top_srcdir)/%D%/bison.help >$@.tmp
	$(AM_V_at)diff -u $@~ $@.tmp || true
	$(AM_V_at)mv $@.tmp $@
MAINTAINERCLEANFILES = $(CROSS_OPTIONS_TEXI)


# Fix Info's @code in @deftype
# https://lists.gnu.org/r/help-texinfo/2019-11/msg00004.html
all-local: $(srcdir)/$(%C%_bison).info.bak
$(srcdir)/$(%C%_bison).info.bak: $(srcdir)/$(%C%_bison).info
	$(AM_V_GEN) $(PERL) -pi.bak -0777	\
	  -e 's{(^ --.*\n(?: {10}.*\n)*)}'	\
	  -e '{'				\
	  -e '  $$def = $$1;'			\
	  -e '  $$def =~ s/‘|’//g;'		\
	  -e '  $$def;'				\
	  -e '}gem;' $(srcdir)/$(%C%_bison).info
	@ touch $@
EXTRA_DIST += $(srcdir)/$(%C%_bison).info.bak
MAINTAINERCLEANFILES += $(srcdir)/$(%C%_bison).info.bak


## ---------- ##
## Ref card.  ##
## ---------- ##

EXTRA_DIST += %D%/refcard.tex
CLEANFILES += %D%/refcard.pdf

%D%/refcard.pdf: %D%/refcard.tex
	$(AM_V_GEN) cd %D% && pdftex $(abs_top_srcdir)/%D%/refcard.tex



## ---------------- ##
## %D%/bison.help.  ##
## ---------------- ##

# Some of our targets (cross-options.texi, bison.1) use "bison --help".
# Since we want to ship the generated file to avoid additional
# requirements over the user environment, we used to not depend on
# src/bison itself, but on src/getargs.c and other files.  Yet, we
# need "bison --help" to work to make help2man happy, so we used to
# include "make src/bison" in the commands.  Then we may have a
# problem with concurrent builds, since one make might be aiming one
# of its jobs at compiling src/bison, and another job at generating
# the man page.  If the latter is faster than the former, then we have
# two makes that concurrently try to compile src/bison.  Doomed to
# failure.
#
# As a simple scheme to get our way out, make a stamp file,
# bison.help, which contains --version then --help.  This file can
# depend on bison, which ensures its correctness.  But update it
# *only* if needed (content changes).  This way, we avoid useless
# compilations of cross-options.texi and bison.1.  At the cost of
# repeated builds of bison.help.

EXTRA_DIST += $(top_srcdir)/%D%/bison.help
if ! CROSS_COMPILING
MAINTAINERCLEANFILES += $(top_srcdir)/%D%/bison.help
$(top_srcdir)/%D%/bison.help: src/bison$(EXEEXT)
	$(AM_V_GEN)$(MKDIR_P) %D%
	$(AM_V_at) LC_ALL=C tests/bison --version >%D%/bison.help.tmp
	$(AM_V_at) LC_ALL=C tests/bison --help | \
## Avoid depending on the path to Bison.
	  sed -e 's,^Usage: .*/bison \[OPTION\],Usage: bison [OPTION],g' \
## Avoid variations in the output depending on whether we are
## on a glibc system.
	      -e '/translation bugs/d'  >>%D%/bison.help.tmp
	$(AM_V_at)$(top_srcdir)/build-aux/move-if-change %D%/bison.help.tmp $@
endif ! CROSS_COMPILING


## ----------- ##
## Man Pages.  ##
## ----------- ##

dist_man_MANS = $(top_srcdir)/%D%/bison.1

EXTRA_DIST += $(dist_man_MANS:.1=.x)
MAINTAINERCLEANFILES += $(dist_man_MANS)

# Differences to ignore when comparing man pages (the date).
remove_time_stamp = \
  sed 's/^\(\.TH[^"]*"[^"]*"[^"]*\)"[^"]*"/\1/'

# Depend on configure to get version number changes.
if ! CROSS_COMPILING
MAN_DEPS = %D%/bison.help %D%/bison.x $(top_srcdir)/configure
endif

# If we don't have help2man, just touch the target.  Maintainers must
# have the real thing, so if there's a .git directory, fail hard.
#
# We shouldn't need this, but on some OS the timestamps in the tarball
# leave us no choice.  See
# https://lists.gnu.org/r/bug-bison/2020-05/msg00055.html.
$(top_srcdir)/%D%/bison.1: $(MAN_DEPS)
	$(AM_V_GEN)if $(HELP2MAN) --version >/dev/null 2>&1; then	\
	  $(HELP2MAN)							\
	    --include=$(top_srcdir)/%D%/bison.x				\
	    --output=$@.tmp tests/bison &&				\
	  { $(remove_time_stamp) $@     >$@a.tmp || true; } &&		\
	    $(remove_time_stamp) $@.tmp >$@b.tmp &&			\
	  if diff $@a.tmp $@b.tmp >/dev/null 2>&1; then			\
	    touch $@;							\
	  else								\
	    mv $@.tmp $@;						\
	  fi &&								\
	  rm -f $@*.tmp;						\
	elif test -d $(srcdir)/.git; then				\
	  echo >&2 "ERROR: $@: help2man is needed";			\
	  exit 1;							\
	else								\
	  touch $@;							\
	fi

if ENABLE_YACC
nodist_man_MANS = %D%/yacc.1
endif

## ----------------------------- ##
## Graphviz examples generation. ##
## ----------------------------- ##

CLEANFILES += $(FIGS_GV:.gv=.eps) $(FIGS_GV:.gv=.pdf) $(FIGS_GV:.gv=.svg)
FIGS_GV =                                               \
  %D%/figs/example.gv                                   \
  %D%/figs/example-reduce.gv %D%/figs/example-shift.gv
EXTRA_DIST +=                                                   \
  $(FIGS_GV) $(FIGS_GV:.gv=.txt)                                \
  $(FIGS_GV:.gv=.eps) $(FIGS_GV:.gv=.pdf) $(FIGS_GV:.gv=.svg)
SUFFIXES += .gv .eps .pdf .svg

.gv.eps:
	$(AM_V_GEN) $(MKDIR_P) `echo "./$@" | sed -e 's,/[^/]*$$,,'`
	$(AM_V_at) $(DOT) -Gmargin=0 -Teps $< >$@.tmp
	$(AM_V_at) mv $@.tmp $@

.gv.pdf:
	$(AM_V_GEN) $(MKDIR_P) `echo "./$@" | sed -e 's,/[^/]*$$,,'`
	$(AM_V_at) $(DOT) -Gmargin=0 -Tpdf $< >$@.tmp
	$(AM_V_at) mv $@.tmp $@

.gv.svg:
	$(AM_V_GEN) $(MKDIR_P) `echo "./$@" | sed -e 's,/[^/]*$$,,'`
	$(AM_V_at) $(DOT) -Gmargin=0 -Tsvg $< >$@.tmp
	$(AM_V_at) mv $@.tmp $@

## -------------- ##
## Doxygenation.  ##
## -------------- ##

DOXYGEN = doxygen

.PHONY: doc html

doc: html

html-local: %D%/Doxyfile
	$(AM_V_GEN) $(DOXYGEN) %D%/Doxyfile

edit = sed -e 's,@PACKAGE_NAME\@,$(PACKAGE_NAME),g' \
	   -e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' \
	   -e 's,@PERL\@,$(PERL),g' \
	   -e 's,@top_builddir\@,$(top_builddir),g' \
	   -e 's,@top_srcdir\@,$(top_srcdir),g'

EXTRA_DIST += %D%/Doxyfile.in
CLEANFILES += %D%/Doxyfile
# Sed is used to generate Doxyfile from Doxyfile.in instead of
# configure, because the former is way faster than the latter.
%D%/Doxyfile: $(top_srcdir)/%D%/Doxyfile.in
	$(AM_V_GEN) $(edit) $(top_srcdir)/%D%/Doxyfile.in >%D%/Doxyfile

CLEANDIRS += %D%/html
