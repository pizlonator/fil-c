/* Test scanf functions with C23 binary integers.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

#include <libc-diag.h>
#include <support/check.h>
#include <support/xstdio.h>

#define CONCAT_(X, Y, Z) X ## Y ## Z
#define CONCAT(X, Y, Z) CONCAT_ (X, Y, Z)
#define FNX(FN1, FN2) CONCAT (FN1, FNW, FN2)
#ifndef STDX
# define STDX ""
#endif

#define INFILE OBJPFX "/tst-" STDX "scanf-binary-" STD "-in"

static int
wrap_vfscanf (FILE *fp, const CHAR *format, ...)
{
  va_list ap;
  va_start (ap, format);
  int ret = FNX (vf, scanf) (fp, format, ap);
  va_end (ap);
  return ret;
}

static int
wrap_vscanf (const CHAR *format, ...)
{
  va_list ap;
  va_start (ap, format);
  int ret = FNX (v, scanf) (format, ap);
  va_end (ap);
  return ret;
}

static int
wrap_vsscanf (const CHAR *s, const CHAR *format, ...)
{
  va_list ap;
  va_start (ap, format);
  int ret = FNX (vs, scanf) (s, format, ap);
  va_end (ap);
  return ret;
}

static void
one_check (const CHAR *s, int expected, char expected_c)
{
  int ret;
  FILE *fp;
  int ret_i;
  long int ret_l;
  long long int ret_ll;
  char ret_c;
  fp = xfopen (INFILE, "w");
  ret = FNX (fput, s) (s, fp);
  TEST_VERIFY_EXIT (0 <= ret);
  xfclose (fp);

  if (!TEST_C23)
    {
      expected = 0;
      expected_c = s[0] == L_('-') ? s[2] : s[1];
    }

  ret = FNX (s, scanf) (s, L_("%i %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = FNX (f, scanf) (fp, L_("%i %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = FNX (, scanf) (L_("%i %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, expected);
  TEST_COMPARE (ret_c, expected_c);
  ret = wrap_vsscanf (s, L_("%i %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = wrap_vfscanf (fp, L_("%i %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = wrap_vscanf (L_("%i %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, expected);
  TEST_COMPARE (ret_c, expected_c);

  ret = FNX (s, scanf) (s, L_("%li %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = FNX (f, scanf) (fp, L_("%li %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = FNX (, scanf) (L_("%li %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, expected);
  TEST_COMPARE (ret_c, expected_c);
  ret = wrap_vsscanf (s, L_("%li %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = wrap_vfscanf (fp, L_("%li %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = wrap_vscanf (L_("%li %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, expected);
  TEST_COMPARE (ret_c, expected_c);

  ret = FNX (s, scanf) (s, L_("%lli %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = FNX (f, scanf) (fp, L_("%lli %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = FNX (, scanf) (L_("%lli %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, expected);
  TEST_COMPARE (ret_c, expected_c);
  ret = wrap_vsscanf (s, L_("%lli %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = wrap_vfscanf (fp, L_("%lli %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = wrap_vscanf (L_("%lli %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, expected);
  TEST_COMPARE (ret_c, expected_c);
}

/* GCC does not know the %b format before GCC 12.  */
DIAG_PUSH_NEEDS_COMMENT;
#if !__GNUC_PREREQ (12, 0)
DIAG_IGNORE_NEEDS_COMMENT (11, "-Wformat");
DIAG_IGNORE_NEEDS_COMMENT (11, "-Wformat-extra-args");
#endif

static void
one_check_b (const CHAR *s, int expected, char expected_c)
{
  int ret;
  FILE *fp;
  unsigned int ret_i;
  unsigned long int ret_l;
  unsigned long long int ret_ll;
  char ret_c;
  fp = xfopen (INFILE, "w");
  ret = FNX (fput, s) (s, fp);
  TEST_VERIFY_EXIT (0 <= ret);
  xfclose (fp);

  ret = FNX (s, scanf) (s, L_("%b %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, (unsigned int) expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = FNX (f, scanf) (fp, L_("%b %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, (unsigned int) expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = FNX (, scanf) (L_("%b %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, (unsigned int) expected);
  TEST_COMPARE (ret_c, expected_c);
  ret = wrap_vsscanf (s, L_("%b %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, (unsigned int) expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = wrap_vfscanf (fp, L_("%b %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, (unsigned int) expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = wrap_vscanf (L_("%b %c"), &ret_i, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_i, (unsigned int) expected);
  TEST_COMPARE (ret_c, expected_c);

  ret = FNX (s, scanf) (s, L_("%lb %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, (unsigned long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = FNX (f, scanf) (fp, L_("%lb %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, (unsigned long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = FNX (, scanf) (L_("%lb %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, (unsigned long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  ret = wrap_vsscanf (s, L_("%lb %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, (unsigned long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = wrap_vfscanf (fp, L_("%lb %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, (unsigned long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = wrap_vscanf (L_("%lb %c"), &ret_l, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_l, (unsigned long int) expected);
  TEST_COMPARE (ret_c, expected_c);

  ret = FNX (s, scanf) (s, L_("%llb %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, (unsigned long long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = FNX (f, scanf) (fp, L_("%llb %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, (unsigned long long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = FNX (, scanf) (L_("%llb %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, (unsigned long long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  ret = wrap_vsscanf (s, L_("%llb %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, (unsigned long long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  fp = xfopen (INFILE, "r");
  ret = wrap_vfscanf (fp, L_("%llb %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, (unsigned long long int) expected);
  TEST_COMPARE (ret_c, expected_c);
  xfclose (fp);
  fp = xfreopen (INFILE, "r", stdin);
  ret = wrap_vscanf (L_("%llb %c"), &ret_ll, &ret_c);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (ret_ll, (unsigned long long int) expected);
  TEST_COMPARE (ret_c, expected_c);
}

#define CHECK_SCNB(TYPE, MACRO, S, EXPECTED, EXPECTED_C)		\
  do									\
    {									\
      int ret;								\
      FILE *fp;								\
      TYPE ret_t;							\
      char ret_c;							\
      fp = xfopen (INFILE, "w");					\
      ret = FNX (fput, s) (S, fp);					\
      TEST_VERIFY_EXIT (0 <= ret);					\
      xfclose (fp);							\
      ret = FNX (s, scanf) (S, L_("%") MACRO " %c", &ret_t, &ret_c);	\
      TEST_COMPARE (ret, 2);						\
      TEST_COMPARE (ret_t, EXPECTED);					\
      TEST_COMPARE (ret_c, EXPECTED_C);					\
      fp = xfopen (INFILE, "r");					\
      ret = FNX (f, scanf) (fp, L_("%") MACRO " %c", &ret_t, &ret_c);	\
      TEST_COMPARE (ret, 2);						\
      TEST_COMPARE (ret_t, EXPECTED);					\
      TEST_COMPARE (ret_c, EXPECTED_C);					\
      xfclose (fp);							\
      fp = xfreopen (INFILE, "r", stdin);				\
      ret = FNX (, scanf) (L_("%") MACRO " %c", &ret_t, &ret_c);	\
      TEST_COMPARE (ret, 2);						\
      TEST_COMPARE (ret_t, EXPECTED);					\
      TEST_COMPARE (ret_c, EXPECTED_C);					\
      ret = wrap_vsscanf (S, L_("%") MACRO " %c", &ret_t, &ret_c);	\
      TEST_COMPARE (ret, 2);						\
      TEST_COMPARE (ret_t, EXPECTED);					\
      TEST_COMPARE (ret_c, EXPECTED_C);					\
      fp = xfopen (INFILE, "r");					\
      ret = wrap_vfscanf (fp, L_("%") MACRO " %c", &ret_t, &ret_c);	\
      TEST_COMPARE (ret, 2);						\
      TEST_COMPARE (ret_t, EXPECTED);					\
      TEST_COMPARE (ret_c, EXPECTED_C);					\
      xfclose (fp);							\
      fp = xfreopen (INFILE, "r", stdin);				\
      ret = wrap_vscanf (L_("%") MACRO " %c", &ret_t, &ret_c);		\
      TEST_COMPARE (ret, 2);						\
      TEST_COMPARE (ret_t, EXPECTED);					\
      TEST_COMPARE (ret_c, EXPECTED_C);					\
    }									\
  while (0)

static void
one_check_scnb (const CHAR *s, int expected, char expected_c)
{
#if TEST_C23 || defined _GNU_SOURCE
  CHECK_SCNB (uint8_t, SCNb8, s, (uint8_t) expected, expected_c);
  CHECK_SCNB (uint16_t, SCNb16, s, (uint16_t) expected, expected_c);
  CHECK_SCNB (uint32_t, SCNb32, s, (uint32_t) expected, expected_c);
  CHECK_SCNB (uint64_t, SCNb64, s, (uint64_t) expected, expected_c);
  CHECK_SCNB (uint_least8_t, SCNbLEAST8, s, (uint_least8_t) expected,
	      expected_c);
  CHECK_SCNB (uint_least16_t, SCNbLEAST16, s, (uint_least16_t) expected,
	      expected_c);
  CHECK_SCNB (uint_least32_t, SCNbLEAST32, s, (uint_least32_t) expected,
	      expected_c);
  CHECK_SCNB (uint_least64_t, SCNbLEAST64, s, (uint_least64_t) expected,
	      expected_c);
  CHECK_SCNB (uint_fast8_t, SCNbFAST8, s, (uint_fast8_t) expected, expected_c);
  CHECK_SCNB (uint_fast16_t, SCNbFAST16, s, (uint_fast16_t) expected,
	      expected_c);
  CHECK_SCNB (uint_fast32_t, SCNbFAST32, s, (uint_fast32_t) expected,
	      expected_c);
  CHECK_SCNB (uint_fast64_t, SCNbFAST64, s, (uint_fast64_t) expected,
	      expected_c);
  CHECK_SCNB (uintmax_t, SCNbMAX, s, (uintmax_t) expected, expected_c);
  CHECK_SCNB (uintptr_t, SCNbPTR, s, (uintptr_t) expected, expected_c);
#endif
}

DIAG_POP_NEEDS_COMMENT;

static int
do_test (void)
{
  one_check (L_("0b101 x"), 5, 'x');
  one_check (L_("0B101 x"), 5, 'x');
  one_check (L_("-0b11111 y"), -31, 'y');
  one_check (L_("-0B11111 y"), -31, 'y');
  one_check_b (L_("0b101 x"), 5, 'x');
  one_check_b (L_("0B101 x"), 5, 'x');
  one_check_b (L_("-0b11111 y"), -31, 'y');
  one_check_b (L_("-0B11111 y"), -31, 'y');
  one_check_b (L_("101 x"), 5, 'x');
  one_check_b (L_("-11111 y"), -31, 'y');
  one_check_scnb (L_("0b101 x"), 5, 'x');
  one_check_scnb (L_("0B101 x"), 5, 'x');
  one_check_scnb (L_("-0b11111 y"), -31, 'y');
  one_check_scnb (L_("-0B11111 y"), -31, 'y');
  one_check_scnb (L_("101 x"), 5, 'x');
  one_check_scnb (L_("-11111 y"), -31, 'y');
  return 0;
}

#include <support/test-driver.c>
