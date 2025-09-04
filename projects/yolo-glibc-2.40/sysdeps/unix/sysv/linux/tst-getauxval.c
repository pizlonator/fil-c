/* Basic test for getauxval.
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

#include <unistd.h>
#include <stdio.h>
#include <support/check.h>
#include <sys/auxv.h>

static int missing;
static int mismatch;

static void
check_nonzero (unsigned long t, const char *s)
{
  unsigned long v = getauxval (t);
  printf ("%s: %lu (0x%lx)\n", s, v, v);
  if (v == 0)
    missing++;
}

static void
check_eq (unsigned long t, const char *s, unsigned long want)
{
  unsigned long v = getauxval (t);
  printf ("%s: %lu want: %lu\n", s, v, want);
  if (v != want)
    mismatch++;
}

#define NZ(x) check_nonzero (x, #x)
#define EQ(x, want) check_eq (x, #x, want)

static int
do_test (void)
{
  /* These auxv entries should be non-zero on Linux.  */
  NZ (AT_PHDR);
  NZ (AT_PHENT);
  NZ (AT_PHNUM);
  NZ (AT_PAGESZ);
  NZ (AT_ENTRY);
  NZ (AT_CLKTCK);
  NZ (AT_RANDOM);
  NZ (AT_EXECFN);
  if (missing)
    FAIL_EXIT1 ("Found %d missing auxv entries.\n", missing);

  /* Check against syscalls.  */
  EQ (AT_UID, getuid ());
  EQ (AT_EUID, geteuid ());
  EQ (AT_GID, getgid ());
  EQ (AT_EGID, getegid ());
  if (mismatch)
    FAIL_EXIT1 ("Found %d mismatching auxv entries.\n", mismatch);

  return 0;
}

#include <support/test-driver.c>
