# Copyright (C) 1991-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

#
#	Makefile for setjmp/longjmp routines
#
subdir	:= setjmp

include ../Makeconfig

headers	:= setjmp.h bits/setjmp.h bits/setjmp2.h \
	   bits/types/struct___jmp_buf_tag.h

routines	:= setjmp sigjmp bsd-setjmp bsd-_setjmp \
		   longjmp __longjmp jmp-unwind

tests		:= tst-setjmp jmpbug bug269-setjmp tst-setjmp-fp \
		   tst-sigsetjmp tst-setjmp-static
tests-static	:= tst-setjmp-static
tests-internal  := \
       tst-setjmp-check

gen-as-const-headers += jmp_buf-macros.sym

include ../Rules

# This test messes the stack on purpose, which triggers longjump fortify
# failure
CFLAGS-bug269-setjmp.c += $(no-fortify-source)

# Exclude fortified routines from being built with _FORTIFY_SOURCE
routines_no_fortify += \
  longjmp \
  # routines_no_fortify

$(objpfx)tst-setjmp-fp: $(libm)
