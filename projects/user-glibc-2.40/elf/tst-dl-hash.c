/* Test dl-hash functions.
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


#include <simple-dl-hash.h>
#include <simple-dl-new-hash.h>
#include <dl-hash.h>
#include <dl-new-hash.h>
#include <support/support.h>
#include <support/check.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned int (*hash_f) (const char *);



static int
do_fill_test (size_t len, int fill, const char *name, hash_f testf,
	      hash_f expecf)
{
  uint32_t expec, res;
  char buf[len + 1];
  memset (buf, fill, len);
  buf[len] = '\0';

  expec = expecf (buf);
  res = testf (buf);
  if (expec != res)
    FAIL_EXIT1 ("FAIL: fill(%d) %s(%zu), %x != %x\n", fill, name, len, expec,
		res);

  return 0;
}

static int
do_fill_tests (size_t len, int fill)
{
  if (do_fill_test (len, fill, "dl_new_hash", &_dl_new_hash,
		    &__simple_dl_new_hash))
    return 1;

  return do_fill_test (len, fill, "dl_elf_hash", &_dl_elf_hash,
		       &__simple_dl_elf_hash);
}

static int
do_rand_test (size_t len, const char *name, hash_f testf, hash_f expecf)
{
  uint32_t expec, res;
  size_t i;
  char buf[len + 1];
  char v;
  for (i = 0; i < len; ++i)
    {
      v = random ();
      if (v == 0)
	v = 1;

      buf[i] = v;
    }
  buf[len] = '\0';

  expec = expecf (buf);
  res = testf (buf);
  if (expec != res)
    FAIL_EXIT1 ("FAIL: random %s(%zu), %x != %x\n", name, len, expec, res);

  return 0;
}

static int
do_rand_tests (size_t len)
{
  if (do_rand_test (len, "dl_new_hash", &_dl_new_hash, &__simple_dl_new_hash))
    return 1;

  return do_rand_test (len, "dl_elf_hash", &_dl_elf_hash, &__simple_dl_elf_hash);
}

static int
do_test (void)
{
  size_t i, j;
  for (i = 0; i < 100; ++i)
    {
      for (j = 0; j < 8192; ++j)
	{
	  if (do_rand_tests (i))
	    return 1;

	  if (do_fill_tests (i, -1) || do_fill_tests (i, 1)
	      || do_fill_tests (i, 0x80) || do_fill_tests (i, 0x88))
	    return 1;
	}
    }
  return 0;
}

#include <support/test-driver.c>
