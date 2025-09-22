## Process this file with automake to create Makefile.in
##
## Copyright (C) 2000-2010, 2013, 2014 Red Hat, Inc.
## Copyright (C) 2012 Tilera Corporation
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
include $(top_srcdir)/config/eu.am
if BUILD_STATIC
AM_CFLAGS += $(fpic_CFLAGS)
endif
AM_CPPFLAGS += -I$(top_srcdir)/libebl -I$(top_srcdir)/libasm \
	   -I$(top_srcdir)/libelf -I$(top_srcdir)/libdw

noinst_LIBRARIES = libebl_backends.a libebl_backends_pic.a

modules = i386 sh x86_64 ia64 alpha arm aarch64 sparc ppc ppc64 s390 \
	  m68k bpf riscv csky loongarch arc

i386_SRCS = i386_init.c i386_symbol.c i386_corenote.c i386_cfi.c \
	    i386_retval.c i386_regs.c i386_auxv.c \
	    i386_initreg.c i386_unwind.c

sh_SRCS = sh_init.c sh_symbol.c sh_corenote.c sh_regs.c sh_retval.c

x86_64_SRCS = x86_64_init.c x86_64_symbol.c x86_64_corenote.c x86_64_cfi.c \
	      x86_64_retval.c x86_64_regs.c x86_64_initreg.c \
	      x86_64_unwind.c x32_corenote.c


ia64_SRCS = ia64_init.c ia64_symbol.c ia64_regs.c ia64_retval.c

alpha_SRCS = alpha_init.c alpha_symbol.c alpha_retval.c alpha_regs.c \
	     alpha_corenote.c alpha_auxv.c

arm_SRCS = arm_init.c arm_symbol.c arm_regs.c arm_corenote.c \
	   arm_auxv.c arm_attrs.c arm_retval.c arm_cfi.c arm_initreg.c \
	   arm_machineflagname.c

aarch64_SRCS = aarch64_init.c aarch64_regs.c aarch64_symbol.c	\
	       aarch64_corenote.c aarch64_retval.c aarch64_cfi.c \
	       aarch64_initreg.c aarch64_unwind.c

sparc_SRCS = sparc_init.c sparc_symbol.c sparc_regs.c sparc_retval.c \
	     sparc_corenote.c sparc64_corenote.c sparc_auxv.c sparc_attrs.c \
             sparc_cfi.c sparc_initreg.c

ppc_SRCS = ppc_init.c ppc_symbol.c ppc_retval.c ppc_regs.c \
	   ppc_corenote.c ppc_auxv.c ppc_attrs.c \
	   ppc_cfi.c ppc_initreg.c

ppc64_SRCS = ppc64_init.c ppc64_symbol.c ppc64_retval.c ppc64_corenote.c \
	     ppc64_unwind.c ppc64_resolve_sym.c

s390_SRCS = s390_init.c s390_symbol.c s390_regs.c s390_retval.c \
	    s390_corenote.c s390x_corenote.c s390_cfi.c s390_initreg.c \
	    s390_unwind.c

m68k_SRCS = m68k_init.c m68k_symbol.c m68k_regs.c \
	    m68k_retval.c m68k_corenote.c m68k_cfi.c m68k_initreg.c

# m68k prstatus core notes are described by a packed structure
# which has not naturally aligned fields. Since we don't access
# these fields directly, but take their offset to be used later
# to extract the data through elfxx_xlatetom/memmove, this isn't
# an issue.
m68k_corenote_no_Wpacked_not_aligned = yes

bpf_SRCS = bpf_init.c bpf_regs.c bpf_symbol.c

riscv_SRCS = riscv_init.c riscv_symbol.c riscv_cfi.c riscv_regs.c \
	     riscv_initreg.c riscv_corenote.c riscv64_corenote.c riscv_retval.c

csky_SRCS = csky_attrs.c csky_init.c csky_symbol.c csky_cfi.c \
	    csky_regs.c csky_initreg.c csky_corenote.c

loongarch_SRCS = loongarch_init.c loongarch_symbol.c loongarch_cfi.c \
	    loongarch_regs.c loongarch_retval.c loongarch_initreg.c \
	    loongarch_unwind.c loongarch_corenote.c

arc_SRCS = arc_init.c arc_symbol.c

libebl_backends_a_SOURCES = $(i386_SRCS) $(sh_SRCS) $(x86_64_SRCS) \
			    $(ia64_SRCS) $(alpha_SRCS) $(arm_SRCS) \
			    $(aarch64_SRCS) $(sparc_SRCS) $(ppc_SRCS) \
			    $(ppc64_SRCS) $(s390_SRCS) \
			    $(m68k_SRCS) $(bpf_SRCS) $(riscv_SRCS) $(csky_SRCS) \
			    $(loongarch_SRCS) $(arc_SRCS)

libebl_backends_pic_a_SOURCES =
am_libebl_backends_pic_a_OBJECTS = $(libebl_backends_a_SOURCES:.c=.os)

noinst_HEADERS = libebl_CPU.h common-reloc.c linux-core-note.c x86_corenote.c
EXTRA_DIST = $(modules:=_reloc.def)

MOSTLYCLEANFILES = $(am_libebl_backends_pic_a_OBJECTS)
