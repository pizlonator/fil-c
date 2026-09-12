/* Test sem_open with missing file.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <semaphore.h>

#include <support/check.h>

int
do_test (void)
{
  sem_unlink ("/glibc-tst-sem18");
  errno = 0;
  sem_t *s = sem_open ("/glibc-tst-sem18", 0);
  TEST_VERIFY (s == SEM_FAILED);
  TEST_COMPARE (errno, ENOENT);
  return 0;
}

#include <support/test-driver.c>
