/* qsort(_r) tests to trigger worst case for quicksort.
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
   <http://www.gnu.org/licenses/>.  */

#include <array_length.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>

typedef enum
{
  Sorted,
  Random,
  Repeated,
  Bitonic,
  Duplicated,
} arraytype_t;

/* Ratio of total of elements which will be repeated.  */
static const double RepeatedRatio = 0.2;

/* Ratio of duplicated element .  */
static const double DuplicatedRatio = 0.4;

struct array_t
{
  arraytype_t type;
  const char *name;
} static const arraytypes[] =
{
  { Sorted,       "Sorted" },
  { Random,       "Random" },
  { Repeated,     "Repeated" },
  { Bitonic,      "Bitonic" },
  { Duplicated,   "Duplicated" },
};

/* Return the index of BASE as interpreted as an array of elements
   of size SIZE.  */
static inline void *
arr (void *base, size_t idx, size_t size)
{
  return (void*)((uintptr_t)base + (idx * size));
}

/* Functions used to check qsort.  */
static int
uint8_t_cmp (const void *a, const void *b)
{
  uint8_t ia = *(uint8_t*)a;
  uint8_t ib = *(uint8_t*)b;
  return (ia > ib) - (ia < ib);
}

static int
uint16_t_cmp (const void *a, const void *b)
{
  uint16_t ia = *(uint16_t*)a;
  uint16_t ib = *(uint16_t*)b;
  return (ia > ib) - (ia < ib);
}

static int
uint32_t_cmp (const void *a, const void *b)
{
  uint32_t ia = *(uint32_t*)a;
  uint32_t ib = *(uint32_t*)b;
  return (ia > ib) - (ia < ib);
}

static int
uint64_t_cmp (const void *a, const void *b)
{
  uint64_t ia = *(uint64_t*)a;
  uint64_t ib = *(uint64_t*)b;
  return (ia > ib) - (ia < ib);
}

#define LARGE_SIZE 47

static int
large_cmp (const void *a, const void *b)
{
  return memcmp (a, b, LARGE_SIZE);
}

/* Function used to check qsort_r.  */
typedef enum
{
  UINT8_CMP_T,
  UINT16_CMP_T,
  UINT32_CMP_T,
  UINT64_CMP_T,
  LARGE_CMP_T
} type_cmp_t;

static type_cmp_t
uint_t_cmp_type (size_t sz)
{
  switch (sz)
    {
      case sizeof (uint8_t):  return UINT8_CMP_T;
      case sizeof (uint16_t): return UINT16_CMP_T;
      case sizeof (uint64_t): return UINT64_CMP_T;
      case sizeof (uint32_t): return UINT32_CMP_T;
      default:                return LARGE_CMP_T;
    }
}

static int
uint_t_cmp (const void *a, const void *b, void *arg)
{
  type_cmp_t type = *(type_cmp_t*) arg;
  switch (type)
    {
    case UINT8_CMP_T:  return uint8_t_cmp (a, b);
    case UINT32_CMP_T: return uint32_t_cmp (a, b);
    case UINT16_CMP_T: return uint16_t_cmp (a, b);
    case UINT64_CMP_T: return uint64_t_cmp (a, b);
    default:           return large_cmp (a, b);
    }
}

static void
seq (void *elem, size_t type_size, int value)
{
  if (type_size == sizeof (uint8_t))
    *(uint8_t*)elem = value;
  else if (type_size == sizeof (uint16_t))
    *(uint16_t*)elem = value;
  else if (type_size == sizeof (uint32_t))
    *(uint32_t*)elem = value;
  else if (type_size == sizeof (uint64_t))
    *(uint64_t*)elem = value;
  else
    memset (elem, value, type_size);
}

static void
fill_array (void *array, void *refarray, size_t nmemb, size_t type_size,
	    arraytype_t type)
{
  size_t size = nmemb * type_size;

  switch (type)
    {
    case Sorted:
      for (size_t i = 0; i < nmemb; i++)
	seq (arr (array, i, type_size), type_size, i);
      break;

    case Random:
      arc4random_buf (array, size);
      break;

    case Repeated:
      {
        arc4random_buf (array, size);

	void *randelem = xmalloc (type_size);
	arc4random_buf (randelem, type_size);

	/* Repeat REPEATED elements (based on RepeatRatio ratio) in the random
	   array.  */
        size_t repeated = (size_t)(nmemb * RepeatedRatio);
	for (size_t i = 0; i < repeated; i++)
	  {
	    size_t pos = arc4random_uniform (nmemb - 1);
	    memcpy (arr (array, pos, type_size), randelem, type_size);
	  }
	free (randelem);
      }
      break;

    case Bitonic:
      {
	size_t i;
        for (i = 0; i < nmemb / 2; i++)
	  seq (arr (array, i, type_size), type_size, i);
        for (     ; i < nmemb;     i++)
	  seq (arr (array, i, type_size), type_size, (nmemb - 1) - i);
      }
      break;

    case Duplicated:
      {
	int randelem1 = arc4random ();
	for (size_t i = 0; i < nmemb; i++)
	  seq (arr (array, i, type_size), type_size, randelem1);

	size_t duplicates = (size_t)(nmemb * DuplicatedRatio);
	int randelem2 = arc4random ();
	for (size_t i = 0; i < duplicates; i++)
	  {
	    size_t pos = arc4random_uniform (nmemb - 1);
	    seq (arr (array, pos, type_size), type_size, randelem2);
	  }
      }
      break;
    }

  memcpy (refarray, array, size);
}

typedef int (*cmpfunc_t)(const void *, const void *);

/* Simple insertion sort to use as reference sort.  */
static void
qsort_r_ref (void *p, size_t n, size_t s, __compar_d_fn_t cmp, void *arg)
{
  if (n <= 1)
    return;

  int i = 1;
  char tmp[s];
  while (i < n)
    {
      memcpy (tmp, arr (p, i, s), s);
      int j = i - 1;
      while (j >= 0 && cmp (arr (p, j, s), tmp, arg) > 0)
	{
	  memcpy (arr (p, j + 1, s), arr (p, j, s), s);
	  j = j - 1;
	}
      memcpy (arr (p, j + 1, s), tmp, s);
      i = i + 1;
    }
}

static void
qsort_ref (void *b, size_t n, size_t s, __compar_fn_t cmp)
{
  return qsort_r_ref (b, n, s, (__compar_d_fn_t) cmp, NULL);
}

/* Check if ARRAY of total NMEMB element of size SIZE is sorted
   based on CMPFUNC.  */
static void
check_array (void *array, void *refarray, size_t nmemb, size_t type_size,
	     cmpfunc_t cmpfunc)
{
  for (size_t i = 1; i < nmemb; i++)
    {
      int ret = cmpfunc (arr (array, i,   type_size),
			 arr (array, i-1, type_size));
      TEST_VERIFY_EXIT (ret >= 0);
    }

  size_t size = nmemb * type_size;
  TEST_COMPARE_BLOB (array, size, refarray, size);
}

static void
check_qsort (void *buf, void *refbuf, size_t nelem, size_t type_size,
	     arraytype_t type, cmpfunc_t cmpfunc)
{
  fill_array (buf, refbuf, nelem, type_size, type);

  qsort (buf, nelem, type_size, cmpfunc);
  qsort_ref (refbuf, nelem, type_size, cmpfunc);

  check_array (buf, refbuf, nelem, type_size, cmpfunc);
}

static void
check_qsort_r (void *buf, void *refbuf, size_t nelem, size_t type_size,
	       arraytype_t type, cmpfunc_t cmpfunc)
{
  fill_array (buf, refbuf, nelem, type_size, type);

  type_cmp_t typecmp = uint_t_cmp_type (type_size);

  qsort_r (buf, nelem, type_size, uint_t_cmp, &typecmp);
  qsort_r_ref (refbuf, nelem, type_size, uint_t_cmp, &typecmp);

  check_array (buf, refbuf, nelem, type_size, cmpfunc);
}

static int
do_test (void)
{
  /* Some random sizes.  */
  static const size_t nelems[] = { 0, 1, 7, 20, 32, 100, 256, 1024, 4256 };
  size_t max_nelems = 0;
  for (int i = 0; i < array_length (nelems); i++)
    if (nelems[i] > max_nelems)
      max_nelems = nelems[i];

  static const struct test_t
    {
      size_t type_size;
      cmpfunc_t cmpfunc;
    }
  tests[] =
    {
      { sizeof (uint8_t),  uint8_t_cmp },
      { sizeof (uint16_t), uint16_t_cmp },
      { sizeof (uint32_t), uint32_t_cmp },
      { sizeof (uint64_t), uint64_t_cmp },
      /* Test swap with large elements.  */
      { LARGE_SIZE,        large_cmp },
    };
  size_t max_type_size = 0;
  for (int i = 0; i < array_length (tests); i++)
    if (tests[i].type_size > max_type_size)
      max_type_size = tests[i].type_size;

  void *buf = reallocarray (NULL, max_nelems, max_type_size);
  TEST_VERIFY_EXIT (buf != NULL);
  void *refbuf = reallocarray (NULL, max_nelems, max_type_size);
  TEST_VERIFY_EXIT (refbuf != NULL);

  for (const struct test_t *test = tests; test < array_end (tests); ++test)
    {
      if (test_verbose > 0)
	printf ("info: testing qsort with type_size=%zu\n", test->type_size);
      for (const struct array_t *arraytype = arraytypes;
	   arraytype < array_end (arraytypes);
	   ++arraytype)
	{
	  if (test_verbose > 0)
            printf ("  distribution=%s\n", arraytype->name);
	  for (const size_t *nelem = nelems;
	       nelem < array_end (nelems);
	       ++nelem)
	    {
	      if (test_verbose > 0)
		printf ("    nelem=%zu, total size=%zu\n", *nelem,
			*nelem * test->type_size);

	      check_qsort (buf, refbuf, *nelem, test->type_size,
			   arraytype->type, test->cmpfunc);
	      check_qsort_r (buf, refbuf, *nelem, test->type_size,
			     arraytype->type, test->cmpfunc);
	   }
	}
    }

  free (buf);
  free (refbuf);

  return 0;
}

#include <support/test-driver.c>
