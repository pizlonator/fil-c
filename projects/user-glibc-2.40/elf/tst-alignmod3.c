/* Check alignment of PT_LOAD segment in a shared library.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <support/check.h>
#include <tst-file-align.h>
#include <tst-stack-align.h>

int foo __attribute__ ((aligned (ALIGN))) = 1;

void
do_load_test (void)
{
  printf ("foo: %p\n", &foo);
  TEST_VERIFY (is_aligned (&foo, ALIGN) == 0);
}
