# Copyright (C) 1991-2026 Free Software Foundation, Inc.
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
#	Sub-makefile for ctype portion of the library.
#
subdir	:= ctype

include ../Makeconfig

headers	:= ctype.h

routines := \
  ctype \
  ctype-c99 \
  ctype-c99_l \
  ctype-extn \
  ctype_l \
  isctype \
  # routines
aux := ctype-info

tests := \
  test_ctype \
  tst-ctype-tls-dlmopen \
  tst-ctype-tls-dlopen-static \
  # tests

tests-static := \
  tst-ctype-tls-dlopen-static \
  # tests-static

modules-names := \
  tst-ctype-tls-mod \
  # modules-names

include ../Rules

$(objpfx)tst-ctype-tls-dlmopen: $(shared-thread-library)
$(objpfx)tst-ctype-tls-dlmopen.out: $(objpfx)tst-ctype-tls-mod.so
$(objpfx)tst-ctype-tls-dlopen-static: $(static-thread-library)
$(objpfx)tst-ctype-tls-dlopen-static.out: $(objpfx)tst-ctype-tls-mod.so
tst-ctype-tls-dlopen-static-ENV = \
  LD_LIBRARY_PATH=$(ld-library-path):$(common-objpfx):$(common-objpfx)elf
