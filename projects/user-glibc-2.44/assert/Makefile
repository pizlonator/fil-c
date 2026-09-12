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
#	Sub-makefile for assert portion of the library.
#
subdir	:= assert

include ../Makeconfig

headers := \
  assert.h
  # headers

routines := \
  __assert \
  __libc_assert_fail \
  assert \
  assert-perr \
  # routines

tests := \
  test-assert \
  test-assert-2 \
  test-assert-c++-variadic \
  test-assert-c99 \
  test-assert-gnu99 \
  test-assert-perr \
  test-assert-variadic \
  tst-assert-c++ \
  tst-assert-g++ \
  tst-assert-sa-2025-0001 \
  # tests

CFLAGS-test-assert-c99.c += -std=c99
CFLAGS-test-assert-gnu99.c += -std=gnu99

ifeq ($(have-cxx-thread_local),yes)
CFLAGS-test-assert-c++-variadic.o = $(test-config-cxxflags-stdcxx26)
LDLIBS-test-assert-c++-variadic = -lstdc++
CFLAGS-tst-assert-c++.o = -std=c++11
LDLIBS-tst-assert-c++ = -lstdc++
CFLAGS-tst-assert-g++.o = -std=gnu++11
LDLIBS-tst-assert-g++ = -lstdc++
else
tests-unsupported += \
  test-assert-c++-variadic \
  tst-assert-c++ \
  tst-assert-g++ \
  # tests-unsupported
endif

include ../Rules
