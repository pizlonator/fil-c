/* Measure strstr functions.
   Copyright (C) 2013-2026 Free Software Foundation, Inc.
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

#define MIN_PAGE_SIZE 131072
#define TEST_MAIN
#define TEST_NAME "strstr"
#include "bench-string.h"

#include "json-lib.h"

static const char input[] =
"This manual is written with the assumption that you are at least "
"somewhat familiar with the C programming language and basic programming "
"concepts.  Specifically, familiarity with ISO standard C (*note ISO "
"C::), rather than “traditional” pre-ISO C dialects, is assumed.\n"

"   The GNU C Library includes several “header files”, each of which "
"provides definitions and declarations for a group of related facilities; "
"this information is used by the C compiler when processing your program. "
"For example, the header file ‘stdio.h’ declares facilities for "
"performing input and output, and the header file ‘string.h’ declares "
"string processing utilities.  The organization of this manual generally "
"follows the same division as the header files.\n"

"   If you are reading this manual for the first time, you should read "
"all of the introductory material and skim the remaining chapters.  There "
"are a _lot_ of functions in the GNU C Library and it’s not realistic to "
"expect that you will be able to remember exactly _how_ to use each and "
"every one of them.  It’s more important to become generally familiar "
"with the kinds of facilities that the library provides, so that when you "
"are writing your programs you can recognize _when_ to make use of "
"library functions, and _where_ in this manual you can find more specific "
"information about them.\n";

/* Simple yet efficient strstr - for needles < 32 bytes it is 2-4 times
   faster than the optimized twoway_strstr.  */
static char *
basic_strstr (const char *s1, const char *s2)
{
  size_t i;
  int c = s2[0];

  if (c == 0)
    return (char*)s1;

  for ( ; s1[0] != '\0'; s1++)
    {
      if (s1[0] != c)
	continue;
      for (i = 1; s2[i] != 0; i++)
	if (s1[i] != s2[i])
	  break;
      if (s2[i] == '\0')
	return (char*)s1;
    }

  return NULL;
}

#define RETURN_TYPE char *
#define AVAILABLE(h, h_l, j, n_l)			\
  (((j) + (n_l) <= (h_l)) \
   || ((h_l) += __strnlen ((void*)((h) + (h_l)), (n_l) + 512), \
       (j) + (n_l) <= (h_l)))
#define CHECK_EOL (1)
#define RET0_IF_0(a) if (!a) goto ret0
#define FASTSEARCH(S,C,N) (void*) strchr ((void*)(S), (C))
#define LONG_NEEDLE_THRESHOLD 32U
#define __strnlen strnlen
#include "string/str-two-way.h"

/* Optimized Two-way implementation from GLIBC 2.29.  */
static char *
twoway_strstr (const char *haystack, const char *needle)
{
  size_t needle_len; /* Length of NEEDLE.  */
  size_t haystack_len; /* Known minimum length of HAYSTACK.  */

  /* Handle empty NEEDLE special case.  */
  if (needle[0] == '\0')
    return (char *) haystack;

  /* Skip until we find the first matching char from NEEDLE.  */
  haystack = strchr (haystack, needle[0]);
  if (haystack == NULL || needle[1] == '\0')
    return (char *) haystack;

  /* Ensure HAYSTACK length is at least as long as NEEDLE length.
     Since a match may occur early on in a huge HAYSTACK, use strnlen
     and read ahead a few cachelines for improved performance.  */
  needle_len = strlen (needle);
  haystack_len = __strnlen (haystack, needle_len + 256);
  if (haystack_len < needle_len)
    return NULL;

  /* Check whether we have a match.  This improves performance since we avoid
     the initialization overhead of the two-way algorithm.  */
  if (memcmp (haystack, needle, needle_len) == 0)
    return (char *) haystack;

  /* Perform the search.  Abstract memory is considered to be an array
     of 'unsigned char' values, not an array of 'char' values.  See
     ISO C 99 section 6.2.6.1.  */
  if (needle_len < LONG_NEEDLE_THRESHOLD)
    return two_way_short_needle ((const unsigned char *) haystack,
				  haystack_len,
				 (const unsigned char *) needle, needle_len);
  return two_way_long_needle ((const unsigned char *) haystack, haystack_len,
			      (const unsigned char *) needle, needle_len);
}

typedef char *(*proto_t) (const char *, const char *);

IMPL (strstr, 1)
IMPL (twoway_strstr, 0)
IMPL (basic_strstr, 0)

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, const char *s1,
	     const char *s2, char *exp_result)
{
  size_t i, iters = INNER_LOOP_ITERS_SMALL / 8;
  timing_t start, stop, cur;
  char *res;

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    res = CALL (impl, s1, s2);
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double) cur / (double) iters);

  if (res != exp_result)
    {
      error (0, 0, "Wrong result in function %s %s %s", impl->name,
	     (res == NULL) ? "(null)" : res,
	     (exp_result == NULL) ? "(null)" : exp_result);
      ret = 1;
    }
}

static void
do_test (json_ctx_t *json_ctx, size_t align1, size_t align2, size_t len1,
	 size_t len2, int fail)
{
  char *s1 = (char *) (buf1 + align1);
  char *s2 = (char *) (buf2 + align2);

  size_t size = sizeof (input) - 1;
  size_t pos = (len1 + len2) % size;

  char *ss2 = s2;
  for (size_t l = len2; l > 0; l = l > size ? l - size : 0)
    {
      size_t t = l > size ? size : l;
      if (pos + t <= size)
	ss2 = mempcpy (ss2, input + pos, t);
      else
	{
	  ss2 = mempcpy (ss2, input + pos, size - pos);
	  ss2 = mempcpy (ss2, input, t - (size - pos));
	}
    }
  s2[len2] = '\0';

  char *ss1 = s1;
  for (size_t l = len1; l > 0; l = l > size ? l - size : 0)
    {
      size_t t = l > size ? size : l;
      memcpy (ss1, input, t);
      ss1 += t;
    }

  if (!fail)
    memcpy (s1 + len1 - len2, s2, len2);
  s1[len1] = '\0';

  /* Remove any accidental matches except for the last if !fail.  */
  for (ss1 = basic_strstr (s1, s2); ss1; ss1 = basic_strstr (ss1 + 1, s2))
    if (fail || ss1 != s1 + len1 - len2)
      ++ss1[len2 / 2];

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "len_haystack", len1);
  json_attr_uint (json_ctx, "len_needle", len2);
  json_attr_uint (json_ctx, "align_haystack", align1);
  json_attr_uint (json_ctx, "align_needle", align2);
  json_attr_uint (json_ctx, "fail", fail);

  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, s1, s2, fail ? NULL : s1 + len1 - len2);

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);

}

/* Test needles which exhibit worst-case performance.  This shows that
   basic_strstr is quadratic and thus unsuitable for large needles.
   On the other hand Two-way and skip table implementations are linear with
   increasing needle sizes.  The slowest cases of the two implementations are
   within a factor of 2 on several different microarchitectures.  */

static void
test_hard_needle (json_ctx_t *json_ctx, size_t ne_len, size_t hs_len)
{
  char *ne = (char *) buf1;
  char *hs = (char *) buf2;

  /* Hard needle for strstr algorithm using skip table.  This results in many
     memcmp calls comparing most of the needle.  */
  {
    memset (ne, 'a', ne_len);
    ne[ne_len] = '\0';
    ne[ne_len - 14] = 'b';

    memset (hs, 'a', hs_len);
    for (size_t i = ne_len; i <= hs_len; i += ne_len)
      {
	hs[i - 5] = 'b';
	hs[i - 62] = 'b';
      }

    json_element_object_begin (json_ctx);
    json_attr_uint (json_ctx, "len_haystack", hs_len);
    json_attr_uint (json_ctx, "len_needle", ne_len);
    json_attr_uint (json_ctx, "align_haystack", 0);
    json_attr_uint (json_ctx, "align_needle", 0);
    json_attr_uint (json_ctx, "fail", 1);
    json_attr_string (json_ctx, "desc", "Difficult skiptable(0)");

    json_array_begin (json_ctx, "timings");

    FOR_EACH_IMPL (impl, 0)
      do_one_test (json_ctx, impl, hs, ne, NULL);

    json_array_end (json_ctx);
    json_element_object_end (json_ctx);
  }

  /* 2nd hard needle for strstr algorithm using skip table.  This results in
     many memcmp calls comparing most of the needle.  */
  {
    memset (ne, 'a', ne_len);
    ne[ne_len] = '\0';
    ne[ne_len - 6] = 'b';

    memset (hs, 'a', hs_len);
    for (size_t i = ne_len; i <= hs_len; i += ne_len)
      {
	hs[i - 5] = 'b';
	hs[i - 6] = 'b';
      }

    json_element_object_begin (json_ctx);
    json_attr_uint (json_ctx, "len_haystack", hs_len);
    json_attr_uint (json_ctx, "len_needle", ne_len);
    json_attr_uint (json_ctx, "align_haystack", 0);
    json_attr_uint (json_ctx, "align_needle", 0);
    json_attr_uint (json_ctx, "fail", 1);
    json_attr_string (json_ctx, "desc", "Difficult skiptable(1)");

    json_array_begin (json_ctx, "timings");

    FOR_EACH_IMPL (impl, 0)
      do_one_test (json_ctx, impl, hs, ne, NULL);

    json_array_end (json_ctx);
    json_element_object_end (json_ctx);
  }

  /* Hard needle for Two-way algorithm - the random input causes a large number
     of branch mispredictions which significantly reduces performance on modern
     micro architectures.  */
  {
    for (int i = 0; i < hs_len; i++)
      hs[i] = (rand () & 255) > 155 ? 'a' : 'b';
    hs[hs_len] = 0;

    memset (ne, 'a', ne_len);
    ne[ne_len - 2] = 'b';
    ne[0] = 'b';
    ne[ne_len] = 0;

    json_element_object_begin (json_ctx);
    json_attr_uint (json_ctx, "len_haystack", hs_len);
    json_attr_uint (json_ctx, "len_needle", ne_len);
    json_attr_uint (json_ctx, "align_haystack", 0);
    json_attr_uint (json_ctx, "align_needle", 0);
    json_attr_uint (json_ctx, "fail", 1);
    json_attr_string (json_ctx, "desc", "Difficult 2-way");

    json_array_begin (json_ctx, "timings");

    FOR_EACH_IMPL (impl, 0)
      do_one_test (json_ctx, impl, hs, ne, NULL);

    json_array_end (json_ctx);
    json_element_object_end (json_ctx);
  }

  /* Hard needle for standard algorithm testing first few characters of
   * needle.  */
  {
    for (int i = 0; i < hs_len; i++)
      hs[i] = (rand () & 255) >= 128 ? 'a' : 'b';
    hs[hs_len] = 0;

    for (int i = 0; i < ne_len; i++)
      {
	if (i % 3 == 0)
	  ne[i] = 'a';
	else if (i % 3 == 1)
	  ne[i] = 'b';
	else
	  ne[i] = 'c';
      }
    ne[ne_len] = 0;

    json_element_object_begin (json_ctx);
    json_attr_uint (json_ctx, "len_haystack", hs_len);
    json_attr_uint (json_ctx, "len_needle", ne_len);
    json_attr_uint (json_ctx, "align_haystack", 0);
    json_attr_uint (json_ctx, "align_needle", 0);
    json_attr_uint (json_ctx, "fail", 1);
    json_attr_string (json_ctx, "desc", "Difficult testing first 2");

    json_array_begin (json_ctx, "timings");

    FOR_EACH_IMPL (impl, 0)
      do_one_test (json_ctx, impl, hs, ne, NULL);

    json_array_end (json_ctx);
    json_element_object_end (json_ctx);
  }

  /* Hard needle for bruteforce algorithms that match first few chars.  */
  {
    memset (hs, 'a', hs_len);
    for (int i = ne_len-1; i < hs_len; i += ne_len)
      hs[i] = 'b';
    hs[hs_len] = 0;

    memset (ne, 'a', ne_len);
    ne[ne_len] = 0;

    json_element_object_begin (json_ctx);
    json_attr_uint (json_ctx, "len_haystack", hs_len);
    json_attr_uint (json_ctx, "len_needle", ne_len);
    json_attr_uint (json_ctx, "align_haystack", 0);
    json_attr_uint (json_ctx, "align_needle", 0);
    json_attr_uint (json_ctx, "fail", 1);
    json_attr_string (json_ctx, "desc", "Difficult bruteforce needle");

    json_array_begin (json_ctx, "timings");

    FOR_EACH_IMPL (impl, 0)
      do_one_test (json_ctx, impl, hs, ne, NULL);

    json_array_end (json_ctx);
    json_element_object_end (json_ctx);
  }
}

static int
test_main (void)
{
  json_ctx_t json_ctx;
  test_init ();

  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);
  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);

  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, TEST_NAME);
  json_attr_string (&json_ctx, "bench-variant", "");

  json_array_begin (&json_ctx, "ifuncs");
  FOR_EACH_IMPL (impl, 0)
    json_element_string (&json_ctx, impl->name);
  json_array_end (&json_ctx);

  json_array_begin (&json_ctx, "results");

  for (size_t hlen = 8; hlen <= 256;)
    for (size_t klen = 1; klen <= 16; klen++)
      {
	do_test (&json_ctx, 1, 3, hlen, klen, 0);
	do_test (&json_ctx, 0, 9, hlen, klen, 1);

	do_test (&json_ctx, 1, 3, hlen + 1, klen, 0);
	do_test (&json_ctx, 0, 9, hlen + 1, klen, 1);

	do_test (&json_ctx, getpagesize () - 15, 9, hlen, klen, 1);
	if (hlen < 64)
	  {
	    hlen += 8;
	  }
	else
	  {
	    hlen += 32;
	  }
      }

  for (size_t hlen = 256; hlen <= 65536; hlen *= 2)
    for (size_t klen = 4; klen <= 256; klen *= 2)
      {
	do_test (&json_ctx, 1, 11, hlen, klen, 0);
	do_test (&json_ctx, 14, 5, hlen, klen, 1);

    do_test (&json_ctx, 1, 11, hlen + 1, klen + 1, 0);
    do_test (&json_ctx, 14, 5, hlen + 1, klen + 1, 1);

	do_test (&json_ctx, 1, 11, hlen + 1, klen, 0);
	do_test (&json_ctx, 14, 5, hlen + 1, klen, 1);

	do_test (&json_ctx, getpagesize () - 15, 5, hlen + 1, klen, 1);
      }

  test_hard_needle (&json_ctx, 64, 65536);
  test_hard_needle (&json_ctx, 256, 65536);
  test_hard_needle (&json_ctx, 1024, 65536);

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>
