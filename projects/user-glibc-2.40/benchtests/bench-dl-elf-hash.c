/* Measure __dl_new_hash runtime
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <dl-hash.h>
#include <elf/simple-dl-hash.h>
#define TEST_FUNC(x, y) _dl_elf_hash (x)
#define SIMPLE_TEST_FUNC(x, y) __simple_dl_elf_hash (x)

#define TEST_NAME "_dl_elf_hash"


#include "bench-hash-funcs.c"
