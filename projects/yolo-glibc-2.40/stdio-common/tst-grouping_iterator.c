/* Test for struct grouping_iterator.
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

/* Rebuild the fail to access internal-only functions.  */
#include <grouping_iterator.c>

#include <stdio.h>
#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>

static void
check (int lineno, const char *groupings,
       const char *input, const char *expected)
{
  if (test_verbose)
    {
      printf ("info: %s:%d: \"%s\" via \"", __FILE__, lineno, input);
      for (const char *p = groupings; *p != 0; ++p)
        printf ("\\%o", *p & 0xff);
      printf ("\" to \"%s\"\n", expected);
    }

  size_t initial_group = strchrnul (expected, '\'') - expected;
  size_t separators = 0;
  for (const char *p = expected; *p != '\0'; ++p)
    separators += *p == '\'';

  size_t digits = strlen (input);
  char *out = xmalloc (2 * digits + 1);

  struct grouping_iterator it;
  TEST_COMPARE (grouping_iterator_setup (&it, digits, groupings),
                strchr (expected, '\'') != NULL);
  TEST_COMPARE (it.remaining, digits);
  TEST_COMPARE (it.remaining_in_current_group, initial_group);
  TEST_COMPARE (it.separators, separators);

  char *p = out;
  while (*input != '\0')
    {
      if (__grouping_iterator_next (&it))
        *p++ = '\'';
      TEST_COMPARE (it.separators, separators);
      *p++ = *input++;
    }
  *p++ = '\0';

  TEST_COMPARE (it.remaining, 0);
  TEST_COMPARE (it.remaining_in_current_group, 0);

  TEST_COMPARE_STRING (out, expected);

  free (out);
}

static int
do_test (void)
{
  check (__LINE__, "", "1", "1");
  check (__LINE__, "", "12", "12");
  check (__LINE__, "", "123", "123");
  check (__LINE__, "", "1234", "1234");

  check (__LINE__, "\3", "1", "1");
  check (__LINE__, "\3", "12", "12");
  check (__LINE__, "\3", "123", "123");
  check (__LINE__, "\3", "1234", "1'234");
  check (__LINE__, "\3", "12345", "12'345");
  check (__LINE__, "\3", "123456", "123'456");
  check (__LINE__, "\3", "1234567", "1'234'567");
  check (__LINE__, "\3", "12345678", "12'345'678");
  check (__LINE__, "\3", "123456789", "123'456'789");
  check (__LINE__, "\3", "1234567890", "1'234'567'890");

  check (__LINE__, "\2\3", "1", "1");
  check (__LINE__, "\2\3", "12", "12");
  check (__LINE__, "\2\3", "123", "1'23");
  check (__LINE__, "\2\3", "1234", "12'34");
  check (__LINE__, "\2\3", "12345", "123'45");
  check (__LINE__, "\2\3", "123456", "1'234'56");
  check (__LINE__, "\2\3", "1234567", "12'345'67");
  check (__LINE__, "\2\3", "12345678", "123'456'78");
  check (__LINE__, "\2\3", "123456789", "1'234'567'89");
  check (__LINE__, "\2\3", "1234567890", "12'345'678'90");

  check (__LINE__, "\3\2", "1", "1");
  check (__LINE__, "\3\2", "12", "12");
  check (__LINE__, "\3\2", "123", "123");
  check (__LINE__, "\3\2", "1234", "1'234");
  check (__LINE__, "\3\2", "12345", "12'345");
  check (__LINE__, "\3\2", "123456", "1'23'456");
  check (__LINE__, "\3\2", "1234567", "12'34'567");
  check (__LINE__, "\3\2", "12345678", "1'23'45'678");
  check (__LINE__, "\3\2", "123456789", "12'34'56'789");
  check (__LINE__, "\3\2", "1234567890", "1'23'45'67'890");

  check (__LINE__, "\3\2\1", "1", "1");
  check (__LINE__, "\3\2\1", "12", "12");
  check (__LINE__, "\3\2\1", "123", "123");
  check (__LINE__, "\3\2\1", "1234", "1'234");
  check (__LINE__, "\3\2\1", "12345", "12'345");
  check (__LINE__, "\3\2\1", "123456", "1'23'456");
  check (__LINE__, "\3\2\1", "1234567", "1'2'34'567");
  check (__LINE__, "\3\2\1", "12345678", "1'2'3'45'678");
  check (__LINE__, "\3\2\1", "123456789", "1'2'3'4'56'789");
  check (__LINE__, "\3\2\1", "1234567890", "1'2'3'4'5'67'890");

  check (__LINE__, "\2\3\1", "1", "1");
  check (__LINE__, "\2\3\1", "12", "12");
  check (__LINE__, "\2\3\1", "123", "1'23");
  check (__LINE__, "\2\3\1", "1234", "12'34");
  check (__LINE__, "\2\3\1", "12345", "123'45");
  check (__LINE__, "\2\3\1", "123456", "1'234'56");
  check (__LINE__, "\2\3\1", "1234567", "1'2'345'67");
  check (__LINE__, "\2\3\1", "12345678", "1'2'3'456'78");
  check (__LINE__, "\2\3\1", "123456789", "1'2'3'4'567'89");
  check (__LINE__, "\2\3\1", "1234567890", "1'2'3'4'5'678'90");

  /* No repeats.  */
  check (__LINE__, "\3\377", "1", "1");
  check (__LINE__, "\3\377", "12", "12");
  check (__LINE__, "\3\377", "123", "123");
  check (__LINE__, "\3\377", "1234", "1'234");
  check (__LINE__, "\3\377", "12345", "12'345");
  check (__LINE__, "\3\377", "123456", "123'456");
  check (__LINE__, "\3\377", "1234567", "1234'567");
  check (__LINE__, "\3\377", "12345678", "12345'678");

  check (__LINE__, "\2\3\377", "1", "1");
  check (__LINE__, "\2\3\377", "12", "12");
  check (__LINE__, "\2\3\377", "123", "1'23");
  check (__LINE__, "\2\3\377", "1234", "12'34");
  check (__LINE__, "\2\3\377", "12345", "123'45");
  check (__LINE__, "\2\3\377", "123456", "1'234'56");
  check (__LINE__, "\2\3\377", "1234567", "12'345'67");
  check (__LINE__, "\2\3\377", "12345678", "123'456'78");
  check (__LINE__, "\2\3\377", "123456789", "1234'567'89");
  check (__LINE__, "\2\3\377", "1234567890", "12345'678'90");

  check (__LINE__, "\3\2\377", "1", "1");
  check (__LINE__, "\3\2\377", "12", "12");
  check (__LINE__, "\3\2\377", "123", "123");
  check (__LINE__, "\3\2\377", "1234", "1'234");
  check (__LINE__, "\3\2\377", "12345", "12'345");
  check (__LINE__, "\3\2\377", "123456", "1'23'456");
  check (__LINE__, "\3\2\377", "1234567", "12'34'567");
  check (__LINE__, "\3\2\377", "12345678", "123'45'678");
  check (__LINE__, "\3\2\377", "123456789", "1234'56'789");
  check (__LINE__, "\3\2\377", "1234567890", "12345'67'890");

  /* Locale-based tests.  */

  locale_t loc;
  struct lc_ctype_data *ctype;
  struct grouping_iterator it;

  loc = newlocale (LC_ALL_MASK, "de_DE.UTF-8", 0);
  TEST_VERIFY_EXIT (loc != 0);
  ctype = loc->__locales[LC_CTYPE]->private;
  TEST_VERIFY (!ctype->outdigit_translation_needed);
  for (int i = 0; i <= 9; ++i)
    TEST_COMPARE (ctype->outdigit_bytes[i], 1);
  TEST_COMPARE (ctype->outdigit_bytes_all_equal, 1);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_NUMERIC, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_MONETARY, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  freelocale (loc);

  loc = newlocale (LC_ALL_MASK, "tg_TJ.UTF-8", 0);
  TEST_VERIFY_EXIT (loc != 0);
  ctype = loc->__locales[LC_CTYPE]->private;
  TEST_VERIFY (!ctype->outdigit_translation_needed);
  for (int i = 0; i <= 9; ++i)
    TEST_COMPARE (ctype->outdigit_bytes[i], 1);
  TEST_COMPARE (ctype->outdigit_bytes_all_equal, 1);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_NUMERIC, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_MONETARY, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  freelocale (loc);

  loc = newlocale (LC_ALL_MASK, "hi_IN.UTF-8", 0);
  TEST_VERIFY_EXIT (loc != 0);
  ctype = loc->__locales[LC_CTYPE]->private;
  TEST_VERIFY (ctype->outdigit_translation_needed);
  for (int i = 0; i <= 9; ++i)
    /* Locale uses Devanagari digits.  */
    TEST_COMPARE (ctype->outdigit_bytes[i], 3);
  TEST_COMPARE (ctype->outdigit_bytes_all_equal, 3);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_NUMERIC, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_MONETARY, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 1);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 2);
  TEST_COMPARE (it.non_repeating_groups, 3);
  TEST_COMPARE (it.separators, 3);
  freelocale (loc);

  loc = newlocale (LC_ALL_MASK, "ps_AF.UTF-8", 0);
  TEST_VERIFY_EXIT (loc != 0);
  ctype = loc->__locales[LC_CTYPE]->private;
  TEST_VERIFY (ctype->outdigit_translation_needed);
  for (int i = 0; i <= 9; ++i)
    /* Locale uses non-ASCII digits.  */
    TEST_COMPARE (ctype->outdigit_bytes[i], 2);
  TEST_COMPARE (ctype->outdigit_bytes_all_equal, 2);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_NUMERIC, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_MONETARY, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  freelocale (loc);

  loc = newlocale (LC_ALL_MASK, "bn_BD.UTF-8", 0);
  TEST_VERIFY_EXIT (loc != 0);
  ctype = loc->__locales[LC_CTYPE]->private;
  TEST_VERIFY (ctype->outdigit_translation_needed);
  for (int i = 0; i <= 9; ++i)
    /* Locale uses Bengali digits.  */
    TEST_COMPARE (ctype->outdigit_bytes[i], 3);
  TEST_COMPARE (ctype->outdigit_bytes_all_equal, 3);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_NUMERIC, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 1);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 2);
  TEST_COMPARE (it.non_repeating_groups, 3);
  TEST_COMPARE (it.separators, 3);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_MONETARY, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 1);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 2);
  TEST_COMPARE (it.non_repeating_groups, 3);
  TEST_COMPARE (it.separators, 3);
  freelocale (loc);

  loc = newlocale (LC_ALL_MASK, "unm_US.UTF-8", 0);
  TEST_VERIFY_EXIT (loc != 0);
  ctype = loc->__locales[LC_CTYPE]->private;
  TEST_VERIFY (!ctype->outdigit_translation_needed);
  for (int i = 0; i <= 9; ++i)
    TEST_COMPARE (ctype->outdigit_bytes[i], 1);
  TEST_COMPARE (ctype->outdigit_bytes_all_equal, 1);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_NUMERIC, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 9);
  TEST_COMPARE (it.separators, 3);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_MONETARY, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  freelocale (loc);

  loc = newlocale (LC_ALL_MASK, "rw_RW.UTF-8", 0);
  TEST_VERIFY_EXIT (loc != 0);
  ctype = loc->__locales[LC_CTYPE]->private;
  TEST_VERIFY (!ctype->outdigit_translation_needed);
  for (int i = 0; i <= 9; ++i)
    TEST_COMPARE (ctype->outdigit_bytes[i], 1);
  TEST_COMPARE (ctype->outdigit_bytes_all_equal, 1);
  /* rw_RW has grouping -1 in LC_NUMERIC */
  TEST_COMPARE (__grouping_iterator_init (&it, LC_NUMERIC, loc, 8), false);
  TEST_COMPARE (it.remaining_in_current_group, 8);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (__grouping_iterator_init (&it, LC_MONETARY, loc, 8), true);
  TEST_COMPARE (it.remaining_in_current_group, 2);
  TEST_COMPARE (it.remaining, 8);
  TEST_COMPARE (*it.groupings, 3);
  TEST_COMPARE (it.non_repeating_groups, 0);
  TEST_COMPARE (it.separators, 2);
  freelocale (loc);

  return 0;
}

#include <support/test-driver.c>
