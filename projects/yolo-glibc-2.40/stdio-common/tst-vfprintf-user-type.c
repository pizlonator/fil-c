/* Test for user-defined types in vfprintf.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

/* This test contains a printf format specifier, %P, with a custom
   type which is a long/double pair.  If a precision is specified,
   this indicates the number of such pairs which constitute the
   argument.  */

#include <array_length.h>
#include <locale.h>
#include <printf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <wchar.h>

/* Initialized by do_test using register_printf_type.  */
static int user_type;

struct two_argument
{
  long i;
  double d;
};

static void
my_va_arg_function (void *mem, va_list *ap)
{
  if (test_verbose > 0)
    printf ("info: %s (%p) called\n", __func__, mem);

  struct two_argument *pair = mem;
  pair->i = va_arg (*ap, long);
  pair->d = va_arg (*ap, double);
}

static int
my_printf_function (FILE *fp, const struct printf_info *info,
                    const void *const *args)
{
  if (test_verbose > 0)
    printf ("info: %s (%p, %p, {%p}@%p) called for %%%lc (prec %d)\n",
            __func__, fp, info, args[0], args, (wint_t) info->spec,
            info->prec);

  TEST_COMPARE (info->wide, fwide (fp, 0) > 0);

  TEST_VERIFY (info->spec == 'P');
  size_t nargs;
  int printed;
  if (info->prec >= 0)
    {
      if (info->wide)
        {
          if (fputwc (L'{', fp) < 0)
            return -1;
          }
      else
        {
          if (fputc ('{', fp) < 0)
            return -1;
        }
      nargs = info->prec;
      printed = 1;
    }
  else
    {
      nargs = 1;
      printed = 0;
    }

  for (size_t i = 0; i < nargs; ++i)
    {
      if (i != 0)
        {
          if (info->wide)
            {
              if (fputwc (L',', fp) < 0)
                return -1;
            }
          else
            {
              if (fputc (',', fp) < 0)
                return -1;
            }
          ++printed;
        }

      /* NB: Triple pointer indirection.  ARGS is an array of void *,
         and those pointers point to a pointer to the memory area
         supplied to my_va_arg_function.  */
      struct two_argument *pair = *(void **) args[i];
      int ret;
      if (info->wide)
        ret = fwprintf (fp, L"(%ld, %f)", pair->i, pair->d);
      else
        ret = fprintf (fp, "(%ld, %f)", pair->i, pair->d);
      if (ret < 0)
        return -1;
      printed += ret;
    }
  if (info->prec >= 0)
    {
      if (info->wide)
        {
          if (fputwc (L'}', fp) < 0)
            return -1;
        }
      else
        {
          if (fputc ('}', fp) < 0)
            return -1;
        }
      ++printed;
    }
  return printed;
}

static int
my_arginfo_function (const struct printf_info *info,
                     size_t n, int *argtypes, int *size)
{
  /* Avoid recursion.  */
  if (info->spec != 'P')
    return -1;
  if (test_verbose > 0)
    printf ("info: %s (%p, %zu, %p, %p) called for %%%lc (prec %d)\n",
            __func__, info, n, argtypes, size, (wint_t) info->spec,
            info->prec);

  TEST_VERIFY_EXIT (n >= 1);
  size_t nargs;
  if (info->prec >= 0)
    nargs = info->prec;
  else
    nargs = 1;

  size_t to_fill = nargs;
  if (to_fill > n)
    to_fill = n;
  for (size_t i = 0; i < to_fill; ++i)
    {
      argtypes[i] = user_type;
      size[i] = sizeof (struct two_argument);
    }
  if (test_verbose > 0)
    printf ("info:   %s return value: %zu\n", __func__, nargs);
  return nargs;
}

static int
do_test (void)
{
  user_type = register_printf_type (my_va_arg_function);
  if (test_verbose > 0)
    printf ("info: allocated user type: %d\n", user_type);
  TEST_VERIFY_EXIT (user_type >= PA_LAST);
  TEST_VERIFY_EXIT (register_printf_specifier
                    ('P', my_printf_function, my_arginfo_function) >= 0);

  /* Alias declaration for asprintf, to avoid the format string
     attribute and the associated warning.  */
#if __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 1
  extern int asprintf_alias (char **, const char *, ...) __asm__ ("__asprintfieee128");
#else
  extern int asprintf_alias (char **, const char *, ...) __asm__ ("asprintf");
#endif
  TEST_VERIFY (asprintf_alias == asprintf);
  char *str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "[[%P]]", 123L, 456.0) >= 0);
  TEST_COMPARE_STRING (str, "[[(123, 456.000000)]]");
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "[[%1$P %1$P]]", 123L, 457.0) >= 0);
  TEST_COMPARE_STRING (str, "[[(123, 457.000000) (123, 457.000000)]]");
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "%1$P %2$P %3$P %4$P %5$P %6$P",
                              1L, 1.0,
                              2L, 2.0,
                              3L, 3.0,
                              4L, 4.0,
                              5L, 6.0,
                              6L, 6.0)
              >= 0);
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "%1$P %2$P %3$P %4$P %5$P %6$P"
                                    "%7$P %8$P %9$P %10$P %11$P %12$P",
                              1L, 1.0,
                              2L, 2.0,
                              3L, 3.0,
                              4L, 4.0,
                              5L, 6.0,
                              6L, 6.0,
                              7L, 7.0,
                              8L, 8.0,
                              9L, 9.0,
                              10L, 10.0,
                              11L, 11.0,
                              12L, 12.0)
              >= 0);
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "%1$P %2$P %3$P %4$P %5$P %6$P"
                                    "%7$P %8$P %9$P %10$P %11$P %12$P"
                                    "%13$P %14$P %15$P %16$P %17$P %18$P",
                              1L, 1.0,
                              2L, 2.0,
                              3L, 3.0,
                              4L, 4.0,
                              5L, 6.0,
                              6L, 6.0,
                              7L, 7.0,
                              8L, 8.0,
                              9L, 9.0,
                              10L, 10.0,
                              11L, 11.0,
                              12L, 12.0,
                              13L, 13.0,
                              14L, 14.0,
                              15L, 15.0,
                              16L, 16.0,
                              17L, 17.0,
                              18L, 18.0)
              >= 0);
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "%1$P %2$P %3$P %4$P %5$P %6$P"
                                    "%7$P %8$P %9$P %10$P %11$P %12$P"
                                    "%13$P %14$P %15$P %16$P %17$P %18$P"
                                    "%19$P %20$P %21$P %22$P %23$P %24$P",
                              1L, 1.0,
                              2L, 2.0,
                              3L, 3.0,
                              4L, 4.0,
                              5L, 6.0,
                              6L, 6.0,
                              7L, 7.0,
                              8L, 8.0,
                              9L, 9.0,
                              10L, 10.0,
                              11L, 11.0,
                              12L, 12.0,
                              13L, 13.0,
                              14L, 14.0,
                              15L, 15.0,
                              16L, 16.0,
                              17L, 17.0,
                              18L, 18.0,
                              19L, 19.0,
                              20L, 20.0,
                              21L, 21.0,
                              22L, 22.0,
                              23L, 23.0,
                              24L, 24.0)
              >= 0);
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "%1$P %2$P %3$P %4$P %5$P %6$P"
                                    "%7$P %8$P %9$P %10$P %11$P %12$P"
                                    "%13$P %14$P %15$P %16$P %17$P %18$P"
                                    "%19$P %20$P %21$P %22$P %23$P %24$P"
                                    "%25$P %26$P %27$P %28$P %29$P %30$P",
                              1L, 1.0,
                              2L, 2.0,
                              3L, 3.0,
                              4L, 4.0,
                              5L, 6.0,
                              6L, 6.0,
                              7L, 7.0,
                              8L, 8.0,
                              9L, 9.0,
                              10L, 10.0,
                              11L, 11.0,
                              12L, 12.0,
                              13L, 13.0,
                              14L, 14.0,
                              15L, 15.0,
                              16L, 16.0,
                              17L, 17.0,
                              18L, 18.0,
                              19L, 19.0,
                              20L, 20.0,
                              21L, 21.0,
                              22L, 22.0,
                              23L, 23.0,
                              24L, 34.0,
                              25L, 25.0,
                              26L, 26.0,
                              27L, 27.0,
                              28L, 28.0,
                              29L, 29.0,
                              30, 30.0)
              >= 0);
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "[[%1$P %1$P]]", 123L, 457.0) >= 0);
  TEST_COMPARE_STRING (str, "[[(123, 457.000000) (123, 457.000000)]]");
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "[[%.1P]]", 1L, 2.0) >= 0);
  TEST_COMPARE_STRING (str, "[[{(1, 2.000000)}]]");
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias (&str, "[[%.2P]]", 1L, 2.0, 3L, 4.0) >= 0);
  TEST_COMPARE_STRING (str, "[[{(1, 2.000000),(3, 4.000000)}]]");
  free (str);

  str = NULL;
  TEST_VERIFY (asprintf_alias
               (&str, "[[%.2P | %.3P]]",
                /* argument 1: */ 1L, 2.0, 3L, 4.0,
                /* argument 2: */ 5L, 6.0, 7L, 8.0, 9L, 10.0)
               >= 0);
  TEST_COMPARE_STRING (str,
                       "[["
                       "{(1, 2.000000),(3, 4.000000)}"
                       " | "
                       "{(5, 6.000000),(7, 8.000000),(9, 10.000000)}"
                       "]]");
  free (str);

  /* The following subtest fails due to bug 21534.  */
#if 0
  str = NULL;
  TEST_VERIFY (asprintf_alias
               (&str, "[[%1$.2P | %2$.3P | %1$.2P]]",
                /* argument 1: */ 1L, 2.0, 3L, 4.0,
                /* argument 2: */ 5L, 6.0, 7L, 8.0, 9L, 10.0)
               >= 0);
  TEST_COMPARE_STRING (str,
                       "[["
                       "{(1, 2.000000),(3, 4.000000)}"
                       " | "
                       "{(5, 6.000000),(7, 8.000000),(9, 10.000000)}"
                       " | "
                       "{(1, 2.000000),(3, 4.000000)}"
                       "]]");
  free (str);
#endif

  /* Wide variants of the tests above.  */

  wchar_t buf[200];
  TEST_VERIFY (swprintf (buf, array_length (buf), L"[[%P]]", 123L, 456.0)
               >= 0);
  TEST_COMPARE_STRING_WIDE (buf, L"[[(123, 456.000000)]]");

  TEST_VERIFY (swprintf (buf, array_length (buf), L"[[%1$P %1$P]]",
                         123L, 457.0) >= 0);
  TEST_COMPARE_STRING_WIDE (buf, L"[[(123, 457.000000) (123, 457.000000)]]");

  TEST_VERIFY (swprintf (buf, array_length (buf), L"[[%.1P]]", 1L, 2.0) >= 0);
  TEST_COMPARE_STRING_WIDE (buf, L"[[{(1, 2.000000)}]]");

  TEST_VERIFY (swprintf (buf, array_length (buf), L"[[%.2P]]",
                         1L, 2.0, 3L, 4.0) >= 0);
  TEST_COMPARE_STRING_WIDE (buf, L"[[{(1, 2.000000),(3, 4.000000)}]]");

  TEST_VERIFY (swprintf
               (buf, array_length (buf), L"[[%.2P | %.3P]]",
                /* argument 1: */ 1L, 2.0, 3L, 4.0,
                /* argument 2: */ 5L, 6.0, 7L, 8.0, 9L, 10.0)
               >= 0);
  TEST_COMPARE_STRING_WIDE (buf,
                            L"[["
                            "{(1, 2.000000),(3, 4.000000)}"
                            " | "
                            "{(5, 6.000000),(7, 8.000000),(9, 10.000000)}"
                            "]]");

  /* The following subtest fails due to bug 21534.  */
#if 0
  TEST_VERIFY (swprintf
               (&buf, array_length (buf), L"[[%1$.2P | %2$.3P | %1$.2P]]",
                /* argument 1: */ 1L, 2.0, 3L, 4.0,
                /* argument 2: */ 5L, 6.0, 7L, 8.0, 9L, 10.0)
               >= 0);
  TEST_COMPARE_STRING_WIDE (buf,
                            L"[["
                            "{(1, 2.000000),(3, 4.000000)}"
                            " | "
                            "{(5, 6.000000),(7, 8.000000),(9, 10.000000)}"
                            " | "
                            "{(1, 2.000000),(3, 4.000000)}"
                            "]]");
#endif

  return 0;
}

#include <support/test-driver.c>
