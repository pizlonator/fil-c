/* Check alignment of PT_LOAD segment in a shared library.
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

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <support/check.h>
#include <tst-file-align.h>
#include "tst-p_align.h"

int foo __attribute__ ((aligned (ALIGN))) = 1;

bool
__attribute__ ((weak, noclone, noinline))
is_aligned_p (void *p, int align)
{
  return (((uintptr_t) p) & (align - 1)) == 0;
}

int
do_load_test (void)
{
  printf ("foo: %p\n", &foo);
  TEST_VERIFY (is_aligned_p (&foo, ALIGN));
  return 0;
}
