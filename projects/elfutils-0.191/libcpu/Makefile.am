## Process this file with automake to create Makefile.in
##
## Copyright (C) 2002-2012 Red Hat, Inc.
## This file is part of elfutils.
##
## This file is free software; you can redistribute it and/or modify
## it under the terms of either
##
##   * the GNU Lesser General Public License as published by the Free
##     Software Foundation; either version 3 of the License, or (at
##     your option) any later version
##
## or
##
##   * the GNU General Public License as published by the Free
##     Software Foundation; either version 2 of the License, or (at
##     your option) any later version
##
## or both in parallel, as here.
##
## elfutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received copies of the GNU General Public License and
## the GNU Lesser General Public License along with this program.  If
## not, see <http://www.gnu.org/licenses/>.
##
include $(top_srcdir)/config/eu.am
AM_CPPFLAGS += -I$(srcdir)/../libelf -I$(srcdir)/../libebl \
	    -I$(srcdir)/../libdw -I$(srcdir)/../libasm
if BUILD_STATIC
AM_CFLAGS += $(fpic_CFLAGS)
endif
AM_CFLAGS += -fdollars-in-identifiers
LEXCOMPILE = $(LEX) $(LFLAGS) $(AM_LFLAGS) -P$(<F:lex.l=)
LEX_OUTPUT_ROOT = lex.$(<F:lex.l=)
AM_YFLAGS = -p$(<F:parse.y=)

noinst_LIBRARIES = libcpu.a libcpu_pic.a

noinst_HEADERS = i386_dis.h i386_mne.h x86_64_dis.h

libcpu_a_SOURCES = i386_disasm.c x86_64_disasm.c bpf_disasm.c riscv_disasm.c

libcpu_pic_a_SOURCES =
am_libcpu_pic_a_OBJECTS = $(libcpu_a_SOURCES:.c=.os)

i386_gendis_SOURCES = i386_gendis.c i386_lex.l i386_parse.y

i386_disasm.o: i386.mnemonics $(srcdir)/i386_dis.h
x86_64_disasm.o: x86_64.mnemonics $(srcdir)/x86_64_dis.h

%_defs: $(srcdir)/defs/i386
	$(AM_V_GEN)m4 -D$* -DDISASSEMBLER $< > $@T
	$(AM_V_at)mv -f $@T $@

if MAINTAINER_MODE
noinst_HEADERS += memory-access.h i386_parse.h i386_data.h

noinst_PROGRAMS = i386_gendis$(EXEEXT)

# i386_gendis doesn't clean up, ignore leaks.
# It is just a build tool to generate source in maintainer mode.
if USE_ADDRESS_SANITIZER
GENDIS_ENV=env ASAN_OPTIONS=detect_leaks=0
else
GENDIS_ENV=
endif

$(srcdir)/%_dis.h: %_defs i386_gendis$(EXEEXT)
	$(AM_V_GEN) $(GENDIS_ENV) ./i386_gendis$(EXEEXT) $< > $@T
	$(AM_V_at)mv -f $@T $@

else

$(srcdir)/%_dis.h:
	@echo '*** missing $@; configure with --enable-maintainer-mode'
	@false

endif

%.mnemonics: %_defs
	$(AM_V_GEN)sed '1,/^%%/d;/^#/d;/^[[:space:]]*$$/d;s/[^:]*:\([^[:space:]]*\).*/MNE(\1)/;s/{[^}]*}//g;/INVALID/d' \
	  $< | sort -u > $@

i386_lex_no_Werror = yes

libeu = ../lib/libeu.a

i386_lex_CFLAGS = -Wno-unused-label -Wno-unused-function -Wno-sign-compare \
		  -Wno-implicit-fallthrough
i386_parse.o: i386_parse.c i386.mnemonics
i386_lex.o: i386_parse.h
i386_gendis_LDADD = $(libeu) -lm $(obstack_LIBS)

i386_parse.h: i386_parse.c ;

bpf_disasm_CFLAGS = -Wno-format-nonliteral

EXTRA_DIST = defs/i386

MOSTLYCLEANFILES = $(am_libcpu_pic_a_OBJECTS)
CLEANFILES += $(foreach P,i386 x86_64,$P_defs $P.mnemonics)
MAINTAINERCLEANFILES = $(foreach P,i386 x86_64, $P_dis.h)
