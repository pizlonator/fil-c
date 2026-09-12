/* Measure memset performance.
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

#define MIN_SIZE 32768
#define MAX_SIZE (1024*1024)
#define NUM_TESTS 16384

#define MIN_PAGE_SIZE (MAX_SIZE + getpagesize())
#define TEST_MAIN
#define TEST_NAME "memset"
#include "bench-string.h"
#include <assert.h>
#include "json-lib.h"

typedef void *(*proto_t) (void *, int, size_t);

IMPL (MEMSET, 1)

typedef struct { uint32_t offset : 20, len : 12; } memset_test_t;
static memset_test_t test_arr[NUM_TESTS];

typedef struct { uint16_t size; uint16_t freq; } freq_data_t;
typedef struct { uint8_t align; uint16_t freq; } align_data_t;

#define SIZE_NUM 65536
#define SIZE_MASK (SIZE_NUM-1)
static uint16_t len_arr[SIZE_NUM];

/* Frequency data for memset sizes up to 4096 bytes based on SPEC2017.  */
static freq_data_t memset_len_freq[] =
{
{40,28817}, {32,15336}, { 16,3823}, {296,3545}, { 24,3454}, {  8,1412},
{292,1202}, { 48, 927}, { 12, 613}, { 11, 539}, {284, 493}, {108, 414},
{ 88, 380}, { 20, 295}, {312, 271}, { 72, 233}, {  2, 200}, {  4, 192},
{ 15, 180}, { 14, 174}, { 13, 160}, { 56, 151}, { 36, 144}, { 64, 140},
{4095,133}, { 10, 130}, {  9, 124}, {  3, 124}, { 28, 120}, {  0, 118},
{288, 110}, {1152, 96}, {104,  90}, {  1,  86}, {832,  76}, {248,  74},
{1024, 69}, {120,  64}, {512,  63}, {384,  60}, {  6,  59}, { 80,  54},
{ 17,  50}, {  7,  49}, {520,  47}, {2048, 39}, {256,  37}, {864,  33},
{1440, 28}, { 22,  27}, {2056, 24}, {260,  23}, { 68,  23}, {  5,  22},
{ 18,  21}, {200,  18}, {2120, 18}, { 60,  17}, { 52,  16}, {336,  15},
{ 44,  13}, {192,  13}, {160,  12}, {2064, 12}, {128,  12}, { 76,  11},
{164,  11}, {152,  10}, {136,   9}, {488,   7}, { 96,   6}, {560,   6},
{1016,  6}, {112,   5}, {232,   5}, {168,   5}, {952,   5}, {184,   5},
{144,   4}, {252,   4}, { 84,   3}, {960,   3}, {3808,  3}, {244,   3},
{280,   3}, {224,   3}, {156,   3}, {1088,  3}, {440,   3}, {216,   2},
{304,   2}, { 23,   2}, { 25,   2}, { 26,   2}, {264,   2}, {328,   2},
{1096,  2}, {240,   2}, {1104,  2}, {704,   2}, {1664,  2}, {360,   2},
{808,   1}, {544,   1}, {236,   1}, {720,   1}, {368,   1}, {424,   1},
{640,   1}, {1112,  1}, {552,   1}, {272,   1}, {776,   1}, {376,   1},
{ 92,   1}, {536,   1}, {824,   1}, {496,   1}, {760,   1}, {792,   1},
{504,   1}, {344,   1}, {1816,  1}, {880,   1}, {176,   1}, {320,   1},
{352,   1}, {2008,  1}, {208,   1}, {408,   1}, {228,   1}, {2072,  1},
{568,   1}, {220,   1}, {616,   1}, {600,   1}, {392,   1}, {696,   1},
{2144,  1}, {1280,  1}, {2136,  1}, {632,   1}, {584,   1}, {456,   1},
{472,   1}, {3440,  1}, {2088,  1}, {680,   1}, {2928,  1}, {212,   1},
{648,   1}, {1752,  1}, {664,   1}, {3512,  1}, {1032,  1}, {528,   1},
{4072,  1}, {204,   1}, {2880,  1}, {3392,  1}, {712,   1}, { 59,   1},
{736,   1}, {592,   1}, {2520,  1}, {744,   1}, {196,   1}, {172,   1},
{728,   1}, {2040,  1}, {1192,  1}, {3600,  1}, {0, 0}
};

#define ALIGN_NUM 1024
#define ALIGN_MASK (ALIGN_NUM-1)
static uint8_t align_arr[ALIGN_NUM];

/* Alignment data for memset based on SPEC2017.  */
static align_data_t memset_align_freq[] =
{
  {16, 338}, {8, 307}, {32, 148}, {64, 131}, {4, 72}, {1, 23}, {2, 5}, {0, 0}
};

static void
init_memset_distribution (void)
{
  int i, j, freq, size, n;

  for (n = i = 0; (freq = memset_len_freq[i].freq) != 0; i++)
    for (j = 0, size = memset_len_freq[i].size; j < freq; j++)
      len_arr[n++] = size;
  assert (n == SIZE_NUM);

  for (n = i = 0; (freq = memset_align_freq[i].freq) != 0; i++)
    for (j = 0, size = memset_align_freq[i].align; j < freq; j++)
      align_arr[n++] = size - 1;
  assert (n == ALIGN_NUM);
}

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, size_t iters,
	     memset_test_t *input, size_t n)
{
  timing_t start, stop, cur;

  /* Avoid 'cold start' performance penalty.  */
  for (int j = 0; j < n; j++)
    CALL (impl, buf1 + input[j].offset, 0, input[j].len);

  TIMING_NOW (start);
  for (int i = 0; i < iters; ++i)
    for (int j = 0; j < n; j++)
      CALL (impl, buf1 + input[j].offset, 0, input[j].len);
  TIMING_NOW (stop);
  TIMING_DIFF (cur, start, stop);
  json_element_double (json_ctx, (double) cur / (double) iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t max_size)
{
  int n;

  /* Create a random set of memset inputs strings with the given length
     and alignment distributions.  */
  for (n = 0; n < NUM_TESTS; n++)
    {
      int align = align_arr[rand () & ALIGN_MASK];
      int len = len_arr[rand () & SIZE_MASK];

      test_arr[n].offset = (rand () & (max_size - 1)) & ~align;
      test_arr[n].len = len;
    }

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "length", (double) max_size);
  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, INNER_LOOP_ITERS_MEDIUM * 2, test_arr, n);

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

int
test_main (void)
{
  json_ctx_t json_ctx;

  test_init ();
  init_memset_distribution ();
  memset (buf1, 1, MIN_PAGE_SIZE);

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
  for (int i = MIN_SIZE; i <= MAX_SIZE; i = i * 2)
    do_test (&json_ctx, i);

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>
