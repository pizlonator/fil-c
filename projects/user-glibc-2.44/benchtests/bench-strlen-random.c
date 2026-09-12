/* Measure strlen performance.
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

#define TEST_MAIN
#define TEST_NAME "strlen"

#define NUM_TESTS 65536
#define MAX_ALIGN 32
#define MAX_STRLEN 128
#define MIN_PAGE_SIZE (2 * getpagesize())

#include "bench-string.h"
#include <assert.h>
#include "json-lib.h"

typedef size_t (*proto_t) (const CHAR *);

size_t memchr_strlen (const CHAR *);

IMPL (memchr_strlen, 0)

size_t
memchr_strlen (const CHAR *p)
{
  return (const CHAR *)MEMCHR (p, 0, PTRDIFF_MAX) - p;
}

IMPL (STRLEN, 1)

static uint32_t strlen_tests[NUM_TESTS];

typedef struct { uint16_t size; uint16_t freq; } freq_data_t;
typedef struct { uint16_t align; uint16_t freq; } align_data_t;

#define SIZE_NUM 65536
#define SIZE_MASK (SIZE_NUM-1)
static uint8_t strlen_len_arr[SIZE_NUM];

/* Frequency data for strlen sizes up to 256 based on SPEC2017.  */
static freq_data_t strlen_len_freq[] =
{
  { 12,22671}, { 18,12834}, { 13, 9555}, {  6, 6348}, { 17, 6095}, { 11, 2115},
  { 10, 1335}, {  7,  814}, {  2,  646}, {  9,  483}, {  8,  471}, { 16,  418},
  {  4,  390}, {  1,  388}, {  5,  233}, {  3,  204}, {  0,   79}, { 14,   79},
  { 15,   69}, { 26,   36}, { 22,   35}, { 31,   24}, { 32,   24}, { 19,   21},
  { 25,   17}, { 28,   15}, { 21,   14}, { 33,   14}, { 20,   13}, { 24,    9},
  { 29,    9}, { 30,    9}, { 23,    7}, { 34,    7}, { 27,    6}, { 44,    5},
  { 42,    4}, { 45,    3}, { 47,    3}, { 40,    2}, { 41,    2}, { 43,    2},
  { 58,    2}, { 78,    2}, { 36,    2}, { 48,    1}, { 52,    1}, { 60,    1},
  { 64,    1}, { 56,    1}, { 76,    1}, { 68,    1}, { 80,    1}, { 84,    1},
  { 72,    1}, { 86,    1}, { 35,    1}, { 39,    1}, { 50,    1}, { 38,    1},
  { 37,    1}, { 46,    1}, { 98,    1}, {102,    1}, {128,    1}, { 51,    1},
  {107,    1}, { 0,     0}
};

#define ALIGN_NUM 1024
#define ALIGN_MASK (ALIGN_NUM-1)
static uint8_t strlen_align_arr[ALIGN_NUM];

/* Alignment data for strlen based on SPEC2017.  */
static align_data_t string_align_freq[] =
{
  {8, 470}, {32, 427}, {16, 99}, {1, 19}, {2, 6}, {4, 3}, {0, 0}
};

static void
init_strlen_distribution (void)
{
  int i, j, freq, size, n;

  for (n = i = 0; (freq = strlen_len_freq[i].freq) != 0; i++)
    for (j = 0, size = strlen_len_freq[i].size; j < freq; j++)
      strlen_len_arr[n++] = size;
  assert (n == SIZE_NUM);

  for (n = i = 0; (freq = string_align_freq[i].freq) != 0; i++)
    for (j = 0, size = string_align_freq[i].align; j < freq; j++)
      strlen_align_arr[n++] = size;
  assert (n == ALIGN_NUM);
}

static volatile size_t maskv = 0;

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, size_t iters,
	     uint32_t *input, size_t n)
{
  timing_t start, stop, cur;
  size_t res = 0;
  size_t mask = maskv;

  /* Avoid 'cold start' performance penalty.  */
  for (int i = 0; i < 10; i++)
    for (int j = 0; j < n; j++)
      CALL (impl, (const char*)buf1 + input[j]);

  TIMING_NOW (start);
  for (int i = 0; i < iters; ++i)
    for (int j = 0; j < n; j++)
      res = CALL (impl, (const char*)buf1 + input[j] + (res & mask));
  TIMING_NOW (stop);
  TIMING_DIFF (cur, start, stop);
  json_element_double (json_ctx, (double) cur / (double) iters);
}

static void
do_test (json_ctx_t *json_ctx)
{
  size_t n;
  uint8_t *a = buf1;
  uint16_t index[MAX_ALIGN];

  memset (a, 'x', MIN_PAGE_SIZE);

  /* Create indices for strings at all alignments.  */
  for (int i = 0; i < MAX_ALIGN; i++)
    {
      index[i] = i * (MAX_STRLEN + 1);
      a[index[i] + MAX_STRLEN] = 0;
    }

  /* Create a random set of strlen input strings using the string length
     and alignment distributions.  */
  for (n = 0; n < NUM_TESTS; n++)
    {
      int align = strlen_align_arr[rand () & ALIGN_MASK];
      int exp_len = strlen_len_arr[rand () & SIZE_MASK];

      strlen_tests[n] =
        index[(align + exp_len) & (MAX_ALIGN - 1)] + MAX_STRLEN - exp_len;
      assert ((strlen_tests[n] & (align - 1)) == 0);
      assert (strlen ((char*) a + strlen_tests[n]) == exp_len);
    }

  json_element_object_begin (json_ctx);
  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, INNER_LOOP_ITERS_MEDIUM, strlen_tests, n);

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

int
test_main (void)
{

  json_ctx_t json_ctx;

  test_init ();
  init_strlen_distribution ();

  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);
  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);

  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, TEST_NAME);
  json_attr_string (&json_ctx, "bench-variant", "random");

  json_array_begin (&json_ctx, "ifuncs");
  FOR_EACH_IMPL (impl, 0)
    json_element_string (&json_ctx, impl->name);
  json_array_end (&json_ctx);

  json_array_begin (&json_ctx, "results");
  do_test (&json_ctx);

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>
