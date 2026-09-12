/* Test skeleton for formatted printf output.
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

/* The following definitions have to be supplied by the source including
   this skeleton:

   Macros:
   MID_WIDTH	Medium width/precision positive integer constant.  Choose
		such as to cause some, but not all the strings produced
		to be truncated for the conversions handled.
   HUGE_WIDTH	Large width/precision positive integer constant.  Choose
		such as to cause none of the strings produced to be
		truncated for the conversions handled.
   REF_FMT	Reference output format string.  Use no flags and such
		a precision and length modifier, where applicable, and
		a conversion as to make sure the output produced allows
		the original value to be reproduced.
   REF_VAL(v)	Reference value V transformation.  For conversions with
		a truncating length modifier define such as to reproduce
		the truncation operation, otherwise let V pass through.
   PREC		[optional] Working precision positive integer constant.
		Set to the number of binary digits in the significand for
		the argument type handled; usually for floating-point
		conversions only, but it may be required for 128-bit or
		wider integer data types as well.

   Typedefs:
   type_t	Variadic function argument type.  Define to the promoted
		type corresponding to the conversion argument type
		handled.

   Variables:
   vals		Array of TYPE_T values.  Choose such as to cover boundary
		and any special cases.
   length	Length modifier string.  Define according to the
		conversion argument type handled.

   The feature to be tested is wrapped into 'printf_under_test'.  It is up
   to the source including this skeleton if this is going to be a macro
   or an actual function.

   See tst-*printf-format-*.c for usage examples.  */

#include <array_length.h>
#include <dlfcn.h>
#include <mcheck.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set to nonzero to select all possible tuples with repetitions of 1..n
   elements from the set of flags as defined in FLAGS array below; n is
   the length of FLAGS array.  Otherwise select all possible tuples with
   repetitions of 1..2 elements, followed by tuples of 3..n elements where
   the index of each element k; k = 2..n in FLAGS is lower than the index
   of element k-1 in FLAGS.  */
#ifndef TST_PRINTF_DUPS
# define TST_PRINTF_DUPS 0
#endif
/* Set to nonzero to report the precision (number of significand digits)
   required for floating-point calculations.  */
#ifndef PREC
# define PREC 0
#endif

/* The list of conversions permitted for the '#' flag, the '0' flag,
   and precision respectively.  */
#define HASH_FORMATS "boxXaAeEfFgG"
#define ZERO_FORMATS "bdiouxXaAeEfFgG"
#define PREC_FORMATS "bdiouxXaAeEfFgGs"

/* Output format conversion flags.  */
static struct
{
  /* Flag character.  */
  char f;
  /* List of conversion specifiers the flag is valid for; NULL if all.  */
  const char *s;
} const flags[] =
  { {'-'}, {'+'}, {' '}, {'#', HASH_FORMATS}, {'0', ZERO_FORMATS} };

/* Helper to initialize elements of the PW array for the width and
   precision to be specified as a positive integer directly in the
   format, and then as both a negative and a positive argument to '*'.  */
#define STR(v) #v
#define WPINIT(v) {0, STR (v)}, {v, NULL}, {-v, NULL}

/* Width and precision settings to iterate over; zero is initialized
   directly as it has no corresponding negated value and other values
   use the helper above.  */
static struct wp
{
  /* Integer argument to '*', used if S is NULL.  */
  int i;
  /* String denoting an integer to use in the format, or NULL to use '*'.  */
  const char *s;
} const wp[] =
  { {0, "0"}, {0, NULL}, WPINIT (1), WPINIT (2),
    WPINIT (MID_WIDTH), WPINIT (HUGE_WIDTH) };

/* Produce a record according to '%' and zero or more output format flags
   already provided in FMT at indices 0..IDX-1, width W if non-NULL, '.'
   precision specifier if POINT set to true, precision P if non-NULL,
   any length modifiers L, conversion C, and value VAL.

   Record formats produced:

   %<FLAGS><L><C>:<VAL>:
   %<FLAGS>.<L><C>:<VAL>:
   %<FLAGS><W><L><C>:<VAL>:
   %<FLAGS><W>.<L><C>:<VAL>:
   %<FLAGS>.<P><L><C>:<VAL>:
   %<FLAGS><W>.<P><L><C>:<VAL>:
   %<FLAGS>*<L><C>:<W>:<VAL>:
   %<FLAGS>*.<L><C>:<W>:<VAL>:
   %<FLAGS>.*<L><C>:<P>:<VAL>:
   %<FLAGS>*.*<L><C>:<W>:<P>:<VAL>:

   Return 0 on success, -1 on failure.  */

static int
do_printf (char *fmt, size_t idx,
	   const struct wp *w, bool point, const struct wp *p,
	   const char *l, char c, type_t val)
{
  int wpval[2] = { 0 };
  size_t nint = 0;
  int result;
  size_t i;

  if (w != NULL)
    {
      if (w->s == NULL)
	{
	  fmt[idx++] = '*';
	  wpval[nint++] = w->i;
	}
      else
	for (i = 0; w->s[i] != '\0'; i++)
	  fmt[idx++] = w->s[i];
    }
  if (point)
    fmt[idx++] = '.';
  if (p != NULL)
    {
      if (p->s == NULL)
	{
	  fmt[idx++] = '*';
	  wpval[nint++] = p->i;
	}
      else
	for (i = 0; p->s[i] != '\0'; i++)
	  fmt[idx++] = p->s[i];
    }
  for (i = 0; length[i] != '\0'; i++)
    fmt[idx++] = length[i];
  fmt[idx++] = c;
  fmt[idx] = ':';
  fmt[idx + 1] = '\0';
  if (fputs (fmt, stdout) == EOF)
    {
      perror ("fputs");
      return -1;
    }
  fmt[idx++] = '\0';
  if (nint > 0)
    {
      result = printf ("%i:", wpval[0]);
      if (result < 0)
	{
	  perror ("printf");
	  return -1;
	}
      if (nint > 1)
	{
	  result = printf ("%i:", wpval[1]);
	  if (result < 0)
	    {
	      perror ("printf");
	      return -1;
	    }
	}
    }
  switch (nint)
    {
    case 0:
      result = printf_under_test (fmt, val);
      break;
    case 1:
      result = printf_under_test (fmt, wpval[0], val);
      break;
    case 2:
      result = printf_under_test (fmt, wpval[0], wpval[1], val);
      break;
    default:
      fputs ("Broken test, nint > 2\n", stderr);
      return -1;
    }
  if (result < 0)
    return -1;
  if (fputs (":\n", stdout) == EOF)
    {
      perror ("fputs");
      return -1;
    }
  return 0;
}

/* Produce a list of records according to '%' and zero or more output
   format flags already provided in FMT at indices 0..IDX-1, iterating
   over widths and precisions defined in global WP array, any length
   modifiers L, conversion C, and value VAL.  Inline '0' is omitted for
   the width, as it is a flag already handled among the flags supplied.
   Precision is omitted where the conversion does not allow it.

   Return 0 on success, -1 on failure.  */

static int
do_printf_flags (char *fmt, size_t idx, const char *l, char c, type_t val)
{
  bool do_prec = strchr (PREC_FORMATS, c) != NULL;
  size_t i;

  if (do_printf (fmt, idx, NULL, false, NULL, l, c, val) < 0)
    return -1;
  if (do_prec && do_printf (fmt, idx, NULL, true, NULL, l, c, val) < 0)
    return -1;
  for (i = 0; i < array_length (wp); i++)
    {
      size_t j;

      if (do_prec && do_printf (fmt, idx, NULL, true, wp + i, l, c, val) < 0)
	return -1;
      /* Inline '0' is a flag rather than width and is handled elsewhere.  */
      if (wp[i].s != NULL && wp[i].s[0] == '0' && wp[i].s[1] == '\0')
	continue;
      if (do_printf (fmt, idx, wp + i, false, NULL, l, c, val) < 0)
	return -1;
      if (do_prec)
	{
	  if (do_printf (fmt, idx, wp + i, true, NULL, l, c, val) < 0)
	    return -1;
	  for (j = 0; j < array_length (wp); j++)
	    if (do_printf (fmt, idx, wp + i, true, wp + j, l, c, val) < 0)
	      return -1;
	}
    }
  return 0;
}

/* Produce a list of records using the formatted output specifier
   supplied in ARGV[1] preceded by any length modifier supplied in
   the global LENGTH variable, iterating over format flags defined
   in the global FLAGS array, and values supplied in the global VALS
   array.  Note that the output specifier supplied is not verified
   against TYPE_T, so undefined behavior will result if this is used
   incorrectly.

   If PREC is nonzero, then this record:

   prec:<PREC>

   is produced at the beginning.  Then for each VAL from VALS a block
   of records is produced starting with:

   val:<VAL>

   where VAL is formatted according to REF_FMT output format.  The
   block continues with records as shown with DO_PRINTF above using
   flags iterated over according to TST_PRINTF_DUPS.

   See the top of this file for the definitions that have to be
   provided by the source including this skeleton.  */

static int
do_test (int argc, char *argv[])
{
  char fmt[100] = {'%'};
  size_t j;
  size_t v;
  char c;

  if (argc < 2 || *argv[1] == '\0')
    {
      fprintf (stderr, "Usage: %s <specifier>\n", basename (argv[0]));
      return EXIT_FAILURE;
    }

  mtrace ();

  if (PREC != 0 && printf ("prec:%i\n", PREC) < 0)
    {
      perror ("printf");
      return EXIT_FAILURE;
    }

  c = *argv[1];
  for (v = 0; v < array_length (vals); v++)
    {
      if (printf ("val:%" REF_FMT "\n", REF_VAL (vals[v])) < 0)
	{
	  perror ("printf");
	  return EXIT_FAILURE;
	}

      if (do_printf_flags (fmt, 1, length, c, vals[v]) < 0)
	return EXIT_FAILURE;
      for (j = 0; j < array_length (flags); j++)
	{
	  bool done = false;
	  size_t i[j + 1];
	  size_t k;

	  memset (i, 0, sizeof (i));
	  while (!done)
	    {
	      bool skip = false;
	      size_t idx = 1;
	      char f;

	      for (k = 0; k <= j; k++)
		{
		  const char *s = flags[i[k]].s;

		  if (s && strchr (s, c) == NULL)
		    skip = true;
		  if (!TST_PRINTF_DUPS && j > 1 && k > 0 && i[k] >= i[k - 1])
		    skip = true;
		  if (skip)
		    break;

		  f = flags[i[k]].f;
		  fmt[idx++] = f;
		}
	      if (!skip && do_printf_flags (fmt, idx, length, c, vals[v]) < 0)
		return EXIT_FAILURE;
	      for (k = 0; k <= j; k++)
		{
		  i[k]++;
		  if (i[k] < array_length (flags))
		    break;
		  else if (k == j)
		    done = true;
		  else
		    i[k] = 0;
		}
	    }
	}
    }

  return EXIT_SUCCESS;
}

/* Interpose 'dladdr' with a stub to speed up malloc tracing.  */

int
dladdr (const void *addr, Dl_info *info)
{
  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
