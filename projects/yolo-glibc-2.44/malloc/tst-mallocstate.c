/* Emulate Emacs heap dumping to test malloc_set_state.
   Copyright (C) 2001-2026 Free Software Foundation, Inc.
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
#include <stdio.h>
#include <string.h>
#include <libc-symbols.h>
#include <shlib-compat.h>
#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>

#include "malloc.h"

/* Make the compatibility symbols availabile to this test case.  */
void *malloc_get_state (void);
compat_symbol_reference (libc, malloc_get_state, malloc_get_state, GLIBC_2_0);
int malloc_set_state (void *);
compat_symbol_reference (libc, malloc_set_state, malloc_set_state, GLIBC_2_0);

#define NBINS 128

static struct
{
  long magic;
  long version;
  void *av[NBINS * 2 + 2];
  char *sbrk_base;
  int sbrked_mem_bytes;
  unsigned long trim_threshold;
  unsigned long top_pad;
  unsigned int n_mmaps_max;
  unsigned long mmap_threshold;
  int check_action;
  unsigned long max_sbrked_mem;
  unsigned long max_total_mem;
  unsigned int n_mmaps;
  unsigned int max_n_mmaps;
  unsigned long mmapped_mem;
  unsigned long max_mmapped_mem;
  int using_malloc_checking;
  unsigned long max_fast;
  unsigned long arena_test;
  unsigned long arena_max;
  unsigned long narenas;
} save_state;

static int
do_test (void)
{
  /* Check the dummy implementations always fail.  */
  TEST_VERIFY_EXIT (malloc_set_state (&save_state) == -1);

  errno = 0;
  TEST_VERIFY_EXIT (malloc_get_state () == NULL);

  TEST_VERIFY_EXIT (errno == ENOSYS);

  return 0;
}

#include <support/test-driver.c>
