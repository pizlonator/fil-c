/* Test large input for sgetsgent (bug 30151).
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <gshadow.h>
#include <stddef.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xmemstream.h>
#include <stdlib.h>

static int
do_test (void)
{
  /* Create a shadow group with 1000 members.  */
  struct xmemstream mem;
  xopen_memstream (&mem);
  const char *passwd = "k+zD0nucwfxAo3sw1NXUj6K5vt5M16+X0TVGdE1uFvq5R8V7efJ";
  fprintf (mem.out, "group-name:%s::m0", passwd);
  for (int i = 1; i < 1000; ++i)
    fprintf (mem.out, ",m%d", i);
  xfclose_memstream (&mem);

  /* Call sgetsgent.  */
  char *input = mem.buffer;
  struct sgrp *e = sgetsgent (input);
  TEST_VERIFY_EXIT (e != NULL);
  TEST_COMPARE_STRING (e->sg_namp, "group-name");
  TEST_COMPARE_STRING (e->sg_passwd, passwd);
  /* No administrators.  */
  TEST_COMPARE_STRING (e->sg_adm[0], NULL);
  /* Check the members list.  */
  for (int i = 0; i < 1000; ++i)
    {
      char *member = xasprintf ("m%d", i);
      TEST_COMPARE_STRING (e->sg_mem[i], member);
      free (member);
    }
  TEST_COMPARE_STRING (e->sg_mem[1000], NULL);

  /* Check that putsgent brings back the input string.  */
  xopen_memstream (&mem);
  TEST_COMPARE (putsgent (e, mem.out), 0);
  xfclose_memstream (&mem);
  /* Compare without the trailing '\n' that putsgent added.  */
  TEST_COMPARE (mem.buffer[mem.length - 1], '\n');
  mem.buffer[mem.length - 1] = '\0';
  TEST_COMPARE_STRING (mem.buffer, input);

  free (mem.buffer);
  free (input);
  return 0;
}

#include <support/test-driver.c>
