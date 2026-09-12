/* Check compile-time definition of TLS_TP_OFFSET against run-time value.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <thread_pointer.h>

#include <dl-tls.h>
#include <link.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <support/check.h>

static __thread char thread_var __attribute__ ((tls_model ("initial-exec")));

static int
do_test (void)
{
  printf ("thread variable address: %p\n", &thread_var);
  printf ("thread pointer address: %p\n", __thread_pointer ());
  printf ("pthread_self address: %p\n", (void *) pthread_self ());
  ptrdiff_t block_offset = ((struct link_map *) _r_debug.r_map)->l_tls_offset;
  printf ("main program TLS block offset: %td\n", block_offset);

  if ((uintptr_t) &thread_var < (uintptr_t) THREAD_SELF)
    {
      puts("TLS variables are located before struct pthread.");
      TEST_COMPARE (((intptr_t) __thread_pointer () - block_offset)
                    - (intptr_t) &thread_var,
                    TLS_TP_OFFSET);
    }
  else
    {
      puts("TLS variables are located after struct pthread.");
      TEST_COMPARE (((intptr_t) __thread_pointer () + block_offset)
                    - (intptr_t) &thread_var,
                    TLS_TP_OFFSET);
    }
  return 0;
}

#include <support/test-driver.c>
