/* Test that C/POSIX and C.UTF-8 are consistent.
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

#include <langinfo.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <support/check.h>

/* Initialized by do_test using newlocale.  */
static locale_t c_utf8;

/* Set to true for second pass.  */
static bool use_nl_langinfo_l;

static void
switch_to_c (void)
{
  if (setlocale (LC_ALL, "C") == NULL)
    FAIL_EXIT1 ("setlocale (LC_ALL, \"C\")");
}

static void
switch_to_c_utf8 (void)
{
  if (setlocale (LC_ALL, "C.UTF-8") == NULL)
    FAIL_EXIT1 ("setlocale (LC_ALL, \"C.UTF-8\")");
}

static char *
str (nl_item item)
{
  if (!use_nl_langinfo_l)
    switch_to_c  ();
  return nl_langinfo (item);
}

static char *
str_utf8 (nl_item item)
{
  if (use_nl_langinfo_l)
    return nl_langinfo_l (item, c_utf8);
  else
    {
      switch_to_c_utf8  ();
      return nl_langinfo (item);
    }
}

static wchar_t *
wstr (nl_item item)
{
  return (wchar_t *) str (item);
}

static wchar_t *
wstr_utf8 (nl_item item)
{
  return (wchar_t *) str_utf8 (item);
}

static int
byte (nl_item item)
{
  return (signed char) *str (item);
}

static int
byte_utf8 (nl_item item)
{
  return (signed char) *str_utf8 (item);
}

static int
word (nl_item item)
{
  union
  {
    char *ptr;
    int word;
  } u;
  u.ptr = str (item);
  return u.word;
}

static int
word_utf8 (nl_item item)
{
  union
  {
    char *ptr;
    int word;
  } u;
  u.ptr = str_utf8 (item);
  return u.word;
}

static void
one_pass (void)
{
  /* LC_TIME.  */
  TEST_COMPARE_STRING (str (ABDAY_1), str_utf8 (ABDAY_1));
  TEST_COMPARE_STRING (str (ABDAY_2), str_utf8 (ABDAY_2));
  TEST_COMPARE_STRING (str (ABDAY_3), str_utf8 (ABDAY_3));
  TEST_COMPARE_STRING (str (ABDAY_4), str_utf8 (ABDAY_4));
  TEST_COMPARE_STRING (str (ABDAY_5), str_utf8 (ABDAY_5));
  TEST_COMPARE_STRING (str (ABDAY_6), str_utf8 (ABDAY_6));
  TEST_COMPARE_STRING (str (ABDAY_7), str_utf8 (ABDAY_7));

  TEST_COMPARE_STRING (str (DAY_1), str_utf8 (DAY_1));
  TEST_COMPARE_STRING (str (DAY_2), str_utf8 (DAY_2));
  TEST_COMPARE_STRING (str (DAY_3), str_utf8 (DAY_3));
  TEST_COMPARE_STRING (str (DAY_4), str_utf8 (DAY_4));
  TEST_COMPARE_STRING (str (DAY_5), str_utf8 (DAY_5));
  TEST_COMPARE_STRING (str (DAY_6), str_utf8 (DAY_6));
  TEST_COMPARE_STRING (str (DAY_7), str_utf8 (DAY_7));

  TEST_COMPARE_STRING (str (ABMON_1), str_utf8 (ABMON_1));
  TEST_COMPARE_STRING (str (ABMON_2), str_utf8 (ABMON_2));
  TEST_COMPARE_STRING (str (ABMON_3), str_utf8 (ABMON_3));
  TEST_COMPARE_STRING (str (ABMON_4), str_utf8 (ABMON_4));
  TEST_COMPARE_STRING (str (ABMON_5), str_utf8 (ABMON_5));
  TEST_COMPARE_STRING (str (ABMON_6), str_utf8 (ABMON_6));
  TEST_COMPARE_STRING (str (ABMON_7), str_utf8 (ABMON_7));
  TEST_COMPARE_STRING (str (ABMON_8), str_utf8 (ABMON_8));
  TEST_COMPARE_STRING (str (ABMON_9), str_utf8 (ABMON_9));
  TEST_COMPARE_STRING (str (ABMON_10), str_utf8 (ABMON_10));
  TEST_COMPARE_STRING (str (ABMON_11), str_utf8 (ABMON_11));
  TEST_COMPARE_STRING (str (ABMON_12), str_utf8 (ABMON_12));

  TEST_COMPARE_STRING (str (MON_1), str_utf8 (MON_1));
  TEST_COMPARE_STRING (str (MON_2), str_utf8 (MON_2));
  TEST_COMPARE_STRING (str (MON_3), str_utf8 (MON_3));
  TEST_COMPARE_STRING (str (MON_4), str_utf8 (MON_4));
  TEST_COMPARE_STRING (str (MON_5), str_utf8 (MON_5));
  TEST_COMPARE_STRING (str (MON_6), str_utf8 (MON_6));
  TEST_COMPARE_STRING (str (MON_7), str_utf8 (MON_7));
  TEST_COMPARE_STRING (str (MON_8), str_utf8 (MON_8));
  TEST_COMPARE_STRING (str (MON_9), str_utf8 (MON_9));
  TEST_COMPARE_STRING (str (MON_10), str_utf8 (MON_10));
  TEST_COMPARE_STRING (str (MON_11), str_utf8 (MON_11));
  TEST_COMPARE_STRING (str (MON_12), str_utf8 (MON_12));

  TEST_COMPARE_STRING (str (AM_STR), str_utf8 (AM_STR));
  TEST_COMPARE_STRING (str (PM_STR), str_utf8 (PM_STR));

  TEST_COMPARE_STRING (str (D_T_FMT), str_utf8 (D_T_FMT));
  TEST_COMPARE_STRING (str (D_FMT), str_utf8 (D_FMT));
  TEST_COMPARE_STRING (str (T_FMT), str_utf8 (T_FMT));
  TEST_COMPARE_STRING (str (T_FMT_AMPM),
                       str_utf8 (T_FMT_AMPM));

  TEST_COMPARE_STRING (str (ERA), str_utf8 (ERA));
  TEST_COMPARE_STRING (str (ERA_YEAR), str_utf8 (ERA_YEAR));
  TEST_COMPARE_STRING (str (ERA_D_FMT), str_utf8 (ERA_D_FMT));
  TEST_COMPARE_STRING (str (ALT_DIGITS), str_utf8 (ALT_DIGITS));
  TEST_COMPARE_STRING (str (ERA_D_T_FMT), str_utf8 (ERA_D_T_FMT));
  TEST_COMPARE_STRING (str (ERA_T_FMT), str_utf8 (ERA_T_FMT));
  TEST_COMPARE (word (_NL_TIME_ERA_NUM_ENTRIES),
                word_utf8 (_NL_TIME_ERA_NUM_ENTRIES));
  /* No array elements, so nothing to compare for _NL_TIME_ERA_ENTRIES.  */
  TEST_COMPARE (word (_NL_TIME_ERA_NUM_ENTRIES), 0);

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABDAY_1), wstr_utf8 (_NL_WABDAY_1));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABDAY_2), wstr_utf8 (_NL_WABDAY_2));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABDAY_3), wstr_utf8 (_NL_WABDAY_3));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABDAY_4), wstr_utf8 (_NL_WABDAY_4));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABDAY_5), wstr_utf8 (_NL_WABDAY_5));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABDAY_6), wstr_utf8 (_NL_WABDAY_6));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABDAY_7), wstr_utf8 (_NL_WABDAY_7));

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WDAY_1), wstr_utf8 (_NL_WDAY_1));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WDAY_2), wstr_utf8 (_NL_WDAY_2));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WDAY_3), wstr_utf8 (_NL_WDAY_3));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WDAY_4), wstr_utf8 (_NL_WDAY_4));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WDAY_5), wstr_utf8 (_NL_WDAY_5));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WDAY_6), wstr_utf8 (_NL_WDAY_6));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WDAY_7), wstr_utf8 (_NL_WDAY_7));

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_1), wstr_utf8 (_NL_WABMON_1));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_2), wstr_utf8 (_NL_WABMON_2));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_3), wstr_utf8 (_NL_WABMON_3));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_4), wstr_utf8 (_NL_WABMON_4));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_5), wstr_utf8 (_NL_WABMON_5));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_6), wstr_utf8 (_NL_WABMON_6));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_7), wstr_utf8 (_NL_WABMON_7));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_8), wstr_utf8 (_NL_WABMON_8));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_9), wstr_utf8 (_NL_WABMON_9));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_10), wstr_utf8 (_NL_WABMON_10));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_11), wstr_utf8 (_NL_WABMON_11));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABMON_12), wstr_utf8 (_NL_WABMON_12));

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_1), wstr_utf8 (_NL_WMON_1));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_2), wstr_utf8 (_NL_WMON_2));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_3), wstr_utf8 (_NL_WMON_3));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_4), wstr_utf8 (_NL_WMON_4));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_5), wstr_utf8 (_NL_WMON_5));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_6), wstr_utf8 (_NL_WMON_6));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_7), wstr_utf8 (_NL_WMON_7));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_8), wstr_utf8 (_NL_WMON_8));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_9), wstr_utf8 (_NL_WMON_9));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_10), wstr_utf8 (_NL_WMON_10));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_11), wstr_utf8 (_NL_WMON_11));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WMON_12), wstr_utf8 (_NL_WMON_12));

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WAM_STR), wstr_utf8 (_NL_WAM_STR));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WPM_STR), wstr_utf8 (_NL_WPM_STR));

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WD_T_FMT), wstr_utf8 (_NL_WD_T_FMT));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WD_FMT), wstr_utf8 (_NL_WD_FMT));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WT_FMT), wstr_utf8 (_NL_WT_FMT));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WT_FMT_AMPM),
                            wstr_utf8 (_NL_WT_FMT_AMPM));

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WERA_YEAR), wstr_utf8 (_NL_WERA_YEAR));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WERA_D_FMT), wstr_utf8 (_NL_WERA_D_FMT));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALT_DIGITS),
                            wstr_utf8 (_NL_WALT_DIGITS));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WERA_D_T_FMT),
                            wstr_utf8 (_NL_WERA_D_T_FMT));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WERA_T_FMT), wstr_utf8 (_NL_WERA_T_FMT));

  /* This is somewhat inconsistent, but see locale/categories.def.  */
  TEST_COMPARE (byte (_NL_TIME_WEEK_NDAYS), byte_utf8 (_NL_TIME_WEEK_NDAYS));
  TEST_COMPARE (word (_NL_TIME_WEEK_1STDAY),
                word_utf8 (_NL_TIME_WEEK_1STDAY));
  TEST_COMPARE (byte (_NL_TIME_WEEK_1STWEEK),
                byte_utf8 (_NL_TIME_WEEK_1STWEEK));
  TEST_COMPARE (byte (_NL_TIME_FIRST_WEEKDAY),
                byte_utf8 (_NL_TIME_FIRST_WEEKDAY));
  TEST_COMPARE (byte (_NL_TIME_FIRST_WORKDAY),
                byte_utf8 (_NL_TIME_FIRST_WORKDAY));
  TEST_COMPARE (byte (_NL_TIME_CAL_DIRECTION),
                byte_utf8 (_NL_TIME_CAL_DIRECTION));
  TEST_COMPARE_STRING (str (_NL_TIME_TIMEZONE), str_utf8 (_NL_TIME_TIMEZONE));

  TEST_COMPARE_STRING (str (_DATE_FMT), str_utf8 (_DATE_FMT));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_W_DATE_FMT), wstr_utf8 (_NL_W_DATE_FMT));

  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_TIME_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_TIME_CODESET), "UTF-8");

  TEST_COMPARE_STRING (str (ALTMON_1), str_utf8 (ALTMON_1));
  TEST_COMPARE_STRING (str (ALTMON_2), str_utf8 (ALTMON_2));
  TEST_COMPARE_STRING (str (ALTMON_3), str_utf8 (ALTMON_3));
  TEST_COMPARE_STRING (str (ALTMON_4), str_utf8 (ALTMON_4));
  TEST_COMPARE_STRING (str (ALTMON_5), str_utf8 (ALTMON_5));
  TEST_COMPARE_STRING (str (ALTMON_6), str_utf8 (ALTMON_6));
  TEST_COMPARE_STRING (str (ALTMON_7), str_utf8 (ALTMON_7));
  TEST_COMPARE_STRING (str (ALTMON_8), str_utf8 (ALTMON_8));
  TEST_COMPARE_STRING (str (ALTMON_9), str_utf8 (ALTMON_9));
  TEST_COMPARE_STRING (str (ALTMON_10), str_utf8 (ALTMON_10));
  TEST_COMPARE_STRING (str (ALTMON_11), str_utf8 (ALTMON_11));
  TEST_COMPARE_STRING (str (ALTMON_12), str_utf8 (ALTMON_12));

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_1), wstr_utf8 (_NL_WALTMON_1));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_2), wstr_utf8 (_NL_WALTMON_2));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_3), wstr_utf8 (_NL_WALTMON_3));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_4), wstr_utf8 (_NL_WALTMON_4));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_5), wstr_utf8 (_NL_WALTMON_5));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_6), wstr_utf8 (_NL_WALTMON_6));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_7), wstr_utf8 (_NL_WALTMON_7));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_8), wstr_utf8 (_NL_WALTMON_8));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_9), wstr_utf8 (_NL_WALTMON_9));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_10), wstr_utf8 (_NL_WALTMON_10));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_11), wstr_utf8 (_NL_WALTMON_11));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WALTMON_12), wstr_utf8 (_NL_WALTMON_12));

  TEST_COMPARE_STRING (str (_NL_ABALTMON_1), str_utf8 (_NL_ABALTMON_1));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_2), str_utf8 (_NL_ABALTMON_2));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_3), str_utf8 (_NL_ABALTMON_3));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_4), str_utf8 (_NL_ABALTMON_4));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_5), str_utf8 (_NL_ABALTMON_5));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_6), str_utf8 (_NL_ABALTMON_6));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_7), str_utf8 (_NL_ABALTMON_7));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_8), str_utf8 (_NL_ABALTMON_8));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_9), str_utf8 (_NL_ABALTMON_9));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_10), str_utf8 (_NL_ABALTMON_10));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_11), str_utf8 (_NL_ABALTMON_11));
  TEST_COMPARE_STRING (str (_NL_ABALTMON_12), str_utf8 (_NL_ABALTMON_12));

  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_1),
                            wstr_utf8 (_NL_WABALTMON_1));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_2),
                            wstr_utf8 (_NL_WABALTMON_2));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_3),
                            wstr_utf8 (_NL_WABALTMON_3));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_4),
                            wstr_utf8 (_NL_WABALTMON_4));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_5),
                            wstr_utf8 (_NL_WABALTMON_5));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_6),
                            wstr_utf8 (_NL_WABALTMON_6));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_7),
                            wstr_utf8 (_NL_WABALTMON_7));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_8),
                            wstr_utf8 (_NL_WABALTMON_8));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_9),
                            wstr_utf8 (_NL_WABALTMON_9));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_10),
                            wstr_utf8 (_NL_WABALTMON_10));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_11),
                            wstr_utf8 (_NL_WABALTMON_11));
  TEST_COMPARE_STRING_WIDE (wstr (_NL_WABALTMON_12),
                            wstr_utf8 (_NL_WABALTMON_12));

  /* LC_COLLATE.  Mostly untested, only expected differences.  */
  TEST_COMPARE_STRING (str (_NL_COLLATE_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_COLLATE_CODESET), "UTF-8");

  /* LC_CTYPE.  Mostly untested, only expected differences.  */
  TEST_COMPARE_STRING (str (CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (CODESET), "UTF-8");

  /* LC_MONETARY.  */
  TEST_COMPARE_STRING (str (INT_CURR_SYMBOL), str_utf8 (INT_CURR_SYMBOL));
  TEST_COMPARE_STRING (str (CURRENCY_SYMBOL), str_utf8 (CURRENCY_SYMBOL));
  TEST_COMPARE_STRING (str (MON_DECIMAL_POINT), str_utf8 (MON_DECIMAL_POINT));
  TEST_COMPARE_STRING (str (MON_THOUSANDS_SEP), str_utf8 (MON_THOUSANDS_SEP));
  TEST_COMPARE_STRING (str (MON_GROUPING), str_utf8 (MON_GROUPING));
  TEST_COMPARE_STRING (str (POSITIVE_SIGN), str_utf8 (POSITIVE_SIGN));
  TEST_COMPARE_STRING (str (NEGATIVE_SIGN), str_utf8 (NEGATIVE_SIGN));
  TEST_COMPARE (byte (INT_FRAC_DIGITS), byte_utf8 (INT_FRAC_DIGITS));
  TEST_COMPARE (byte (FRAC_DIGITS), byte_utf8 (FRAC_DIGITS));
  TEST_COMPARE (byte (P_CS_PRECEDES), byte_utf8 (P_CS_PRECEDES));
  TEST_COMPARE (byte (P_SEP_BY_SPACE), byte_utf8 (P_SEP_BY_SPACE));
  TEST_COMPARE (byte (N_CS_PRECEDES), byte_utf8 (N_CS_PRECEDES));
  TEST_COMPARE (byte (N_SEP_BY_SPACE), byte_utf8 (N_SEP_BY_SPACE));
  TEST_COMPARE (byte (P_SIGN_POSN), byte_utf8 (P_SIGN_POSN));
  TEST_COMPARE (byte (N_SIGN_POSN), byte_utf8 (N_SIGN_POSN));
  TEST_COMPARE_STRING (str (CRNCYSTR), str_utf8 (CRNCYSTR));
  TEST_COMPARE (byte (INT_P_CS_PRECEDES), byte_utf8 (INT_P_CS_PRECEDES));
  TEST_COMPARE (byte (INT_P_SEP_BY_SPACE), byte_utf8 (INT_P_SEP_BY_SPACE));
  TEST_COMPARE (byte (INT_N_CS_PRECEDES), byte_utf8 (INT_N_CS_PRECEDES));
  TEST_COMPARE (byte (INT_N_SEP_BY_SPACE), byte_utf8 (INT_N_SEP_BY_SPACE));
  TEST_COMPARE (byte (INT_P_SIGN_POSN), byte_utf8 (INT_P_SIGN_POSN));
  TEST_COMPARE (byte (INT_N_SIGN_POSN), byte_utf8 (INT_N_SIGN_POSN));
  TEST_COMPARE_STRING (str (_NL_MONETARY_DUO_INT_CURR_SYMBOL),
                       str_utf8 (_NL_MONETARY_DUO_INT_CURR_SYMBOL));
  TEST_COMPARE_STRING (str (_NL_MONETARY_DUO_CURRENCY_SYMBOL),
                       str_utf8 (_NL_MONETARY_DUO_CURRENCY_SYMBOL));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_FRAC_DIGITS),
                byte_utf8 (_NL_MONETARY_DUO_INT_FRAC_DIGITS));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_FRAC_DIGITS),
                byte_utf8 (_NL_MONETARY_DUO_FRAC_DIGITS));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_P_CS_PRECEDES),
                byte_utf8 (_NL_MONETARY_DUO_P_CS_PRECEDES));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_P_SEP_BY_SPACE),
                byte_utf8 (_NL_MONETARY_DUO_P_SEP_BY_SPACE));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_N_CS_PRECEDES),
                byte_utf8 (_NL_MONETARY_DUO_N_CS_PRECEDES));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_N_SEP_BY_SPACE),
                byte_utf8 (_NL_MONETARY_DUO_N_SEP_BY_SPACE));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_P_CS_PRECEDES),
                byte_utf8 (_NL_MONETARY_DUO_INT_P_CS_PRECEDES));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_P_SEP_BY_SPACE),
                byte_utf8 (_NL_MONETARY_DUO_INT_P_SEP_BY_SPACE));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_N_CS_PRECEDES),
                byte_utf8 (_NL_MONETARY_DUO_INT_N_CS_PRECEDES));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_N_SEP_BY_SPACE),
                byte_utf8 (_NL_MONETARY_DUO_INT_N_SEP_BY_SPACE));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_P_SIGN_POSN),
                byte_utf8 (_NL_MONETARY_DUO_INT_P_SIGN_POSN));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_N_SIGN_POSN),
                byte_utf8 (_NL_MONETARY_DUO_INT_N_SIGN_POSN));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_P_SIGN_POSN),
                byte_utf8 (_NL_MONETARY_DUO_P_SIGN_POSN));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_N_SIGN_POSN),
                byte_utf8 (_NL_MONETARY_DUO_N_SIGN_POSN));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_P_SIGN_POSN),
                byte_utf8 (_NL_MONETARY_DUO_INT_P_SIGN_POSN));
  TEST_COMPARE (byte (_NL_MONETARY_DUO_INT_N_SIGN_POSN),
                byte_utf8 (_NL_MONETARY_DUO_INT_N_SIGN_POSN));
  TEST_COMPARE (word (_NL_MONETARY_UNO_VALID_FROM),
                word_utf8 (_NL_MONETARY_UNO_VALID_FROM));
  TEST_COMPARE (word (_NL_MONETARY_UNO_VALID_TO),
                word_utf8 (_NL_MONETARY_UNO_VALID_TO));
  TEST_COMPARE (word (_NL_MONETARY_DUO_VALID_FROM),
                word_utf8 (_NL_MONETARY_DUO_VALID_FROM));
  TEST_COMPARE (word (_NL_MONETARY_DUO_VALID_TO),
                word_utf8 (_NL_MONETARY_DUO_VALID_TO));
  /* _NL_MONETARY_CONVERSION_RATE cannot be tested (word array).  */
  TEST_COMPARE (word (_NL_MONETARY_DECIMAL_POINT_WC),
                word_utf8 (_NL_MONETARY_DECIMAL_POINT_WC));
  TEST_COMPARE (word (_NL_MONETARY_THOUSANDS_SEP_WC),
                word_utf8 (_NL_MONETARY_THOUSANDS_SEP_WC));
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_MONETARY_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_MONETARY_CODESET), "UTF-8");

  /* LC_NUMERIC.  */

  TEST_COMPARE_STRING (str (DECIMAL_POINT), str_utf8 (DECIMAL_POINT));
  TEST_COMPARE_STRING (str (RADIXCHAR), str_utf8 (RADIXCHAR));
  TEST_COMPARE_STRING (str (THOUSANDS_SEP), str_utf8 (THOUSANDS_SEP));
  TEST_COMPARE_STRING (str (THOUSEP), str_utf8 (THOUSEP));
  TEST_COMPARE_STRING (str (GROUPING), str_utf8 (GROUPING));
  TEST_COMPARE (word (_NL_NUMERIC_DECIMAL_POINT_WC),
                word_utf8 (_NL_NUMERIC_DECIMAL_POINT_WC));
  TEST_COMPARE (word (_NL_NUMERIC_THOUSANDS_SEP_WC),
                word_utf8 (_NL_NUMERIC_THOUSANDS_SEP_WC));
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_NUMERIC_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_NUMERIC_CODESET), "UTF-8");

  /* LC_MESSAGES.  */

  TEST_COMPARE_STRING (str (YESEXPR), str_utf8 (YESEXPR));
  TEST_COMPARE_STRING (str (NOEXPR), str_utf8 (NOEXPR));
  TEST_COMPARE_STRING (str (YESSTR), str_utf8 (YESSTR));
  TEST_COMPARE_STRING (str (NOSTR), str_utf8 (NOSTR));
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_MESSAGES_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_MESSAGES_CODESET), "UTF-8");

  /* LC_PAPER.  */

  TEST_COMPARE (word (_NL_PAPER_HEIGHT), word_utf8 (_NL_PAPER_HEIGHT));
  TEST_COMPARE (word (_NL_PAPER_WIDTH), word_utf8 (_NL_PAPER_WIDTH));
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_PAPER_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_PAPER_CODESET), "UTF-8");

  /* LC_NAME.  */

  TEST_COMPARE_STRING (str (_NL_NAME_NAME_FMT),
                       str_utf8 (_NL_NAME_NAME_FMT));
  TEST_COMPARE_STRING (str (_NL_NAME_NAME_GEN),
                       str_utf8 (_NL_NAME_NAME_GEN));
  TEST_COMPARE_STRING (str (_NL_NAME_NAME_MR),
                       str_utf8 (_NL_NAME_NAME_MR));
  TEST_COMPARE_STRING (str (_NL_NAME_NAME_MRS),
                       str_utf8 (_NL_NAME_NAME_MRS));
  TEST_COMPARE_STRING (str (_NL_NAME_NAME_MISS),
                       str_utf8 (_NL_NAME_NAME_MISS));
  TEST_COMPARE_STRING (str (_NL_NAME_NAME_MS),
                       str_utf8 (_NL_NAME_NAME_MS));
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_NAME_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_NAME_CODESET), "UTF-8");

  /* LC_ADDRESS.  */

  TEST_COMPARE_STRING (str (_NL_ADDRESS_POSTAL_FMT),
                       str_utf8 (_NL_ADDRESS_POSTAL_FMT));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_COUNTRY_NAME),
                       str_utf8 (_NL_ADDRESS_COUNTRY_NAME));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_COUNTRY_POST),
                       str_utf8 (_NL_ADDRESS_COUNTRY_POST));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_COUNTRY_AB2),
                       str_utf8 (_NL_ADDRESS_COUNTRY_AB2));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_COUNTRY_AB3),
                       str_utf8 (_NL_ADDRESS_COUNTRY_AB3));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_COUNTRY_CAR),
                       str_utf8 (_NL_ADDRESS_COUNTRY_CAR));
  TEST_COMPARE (word (_NL_ADDRESS_COUNTRY_NUM),
                word_utf8 (_NL_ADDRESS_COUNTRY_NUM));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_COUNTRY_ISBN),
                       str_utf8 (_NL_ADDRESS_COUNTRY_ISBN));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_LANG_NAME),
                       str_utf8 (_NL_ADDRESS_LANG_NAME));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_LANG_AB),
                       str_utf8 (_NL_ADDRESS_LANG_AB));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_LANG_TERM),
                       str_utf8 (_NL_ADDRESS_LANG_TERM));
  TEST_COMPARE_STRING (str (_NL_ADDRESS_LANG_LIB),
                       str_utf8 (_NL_ADDRESS_LANG_LIB));
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_ADDRESS_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_ADDRESS_CODESET), "UTF-8");

  /* LC_TELEPHONE.  */

  TEST_COMPARE_STRING (str (_NL_TELEPHONE_TEL_INT_FMT),
                       str_utf8 (_NL_TELEPHONE_TEL_INT_FMT));
  TEST_COMPARE_STRING (str (_NL_TELEPHONE_TEL_DOM_FMT),
                       str_utf8 (_NL_TELEPHONE_TEL_DOM_FMT));
  TEST_COMPARE_STRING (str (_NL_TELEPHONE_INT_SELECT),
                       str_utf8 (_NL_TELEPHONE_INT_SELECT));
  TEST_COMPARE_STRING (str (_NL_TELEPHONE_INT_PREFIX),
                       str_utf8 (_NL_TELEPHONE_INT_PREFIX));
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_TELEPHONE_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_TELEPHONE_CODESET), "UTF-8");

  /* LC_MEASUREMENT.  */

  TEST_COMPARE (byte (_NL_MEASUREMENT_MEASUREMENT),
                byte_utf8 (_NL_MEASUREMENT_MEASUREMENT));
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_MEASUREMENT_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_MEASUREMENT_CODESET), "UTF-8");

  /* LC_IDENTIFICATION is skipped since C.UTF-8 is distinct from C.  */

  /* _NL_IDENTIFICATION_CATEGORY cannot be tested because it is a
     string array.  */
  /* Expected difference.  */
  TEST_COMPARE_STRING (str (_NL_IDENTIFICATION_CODESET), "ANSI_X3.4-1968");
  TEST_COMPARE_STRING (str_utf8 (_NL_IDENTIFICATION_CODESET), "UTF-8");
}

static int
do_test (void)
{
  puts ("info: using setlocale and nl_langinfo");
  one_pass ();

  puts ("info: using nl_langinfo_l");

  c_utf8 = newlocale (LC_ALL_MASK, "C.UTF-8", (locale_t) 0);
  TEST_VERIFY_EXIT (c_utf8 != (locale_t) 0);

  switch_to_c ();
  use_nl_langinfo_l = true;
  one_pass ();

  freelocale (c_utf8);

  return 0;
}

#include <support/test-driver.c>
