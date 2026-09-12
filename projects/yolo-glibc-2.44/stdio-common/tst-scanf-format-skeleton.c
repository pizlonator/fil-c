/* Test skeleton for formatted scanf input.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
   TYPE_T_UNSIGNED_P	[optional] Set to 1 if handling an unsigned
			integer conversion.

   Typedefs:
   type_t		Type to hold data produced by the conversion
			handled.

   Callable objects:
   scanf_under_test	Wrapper for the 'scanf' family feature to be
			tested.
   verify_input		Verifier called to determine whether there is a
			match between the data retrieved by the feature
			tested and MATCH reference data supplied by input.
   pointer_to_value	Converter making a pointer suitable for the
			feature tested from the data holding type.
   initialize_value	Initializer for the data holder to use ahead of
			each call to the feature tested.

   It is up to the source including this skeleton whether the individual
   callable objects are going to be macros or actual functions.

   See tst-*scanf-format-*.c for usage examples.  */

#include <ctype.h>
#include <dlfcn.h>
#include <mcheck.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <support/check.h>
#include <support/support.h>

/* Tweak our environment according to any TYPE_T_UNSIGNED_P setting
   supplied by the individual test case.  */
#ifndef TYPE_T_UNSIGNED_P
# define TYPE_T_UNSIGNED_P 0
#endif

/* Read and return a single character from standard input, returning
   end-of-file or error status indication where applicable.  */

static int
read_input (void)
{
  int c = getchar ();
  if (ferror (stdin))
    c = INPUT_ERROR;
  else if (feof (stdin))
    c = INPUT_EOF;
  return c;
}

/* Consume a signed decimal integer supplied by READ_INPUT above, up to
   the following ':' field separator which is removed from input, making
   sure the value requested does not overflow the range of the data type
   according to TYPE_T_UNSIGNED_P.

   Return the value retrieved and set ERRP to zero on success, otherwise
   set ERRP to the error code.  */

static long long
read_integer (int *errp)
{
  bool m = false;
  int ch;

  ch = read_input ();
  if (ch == '-' || ch == '+')
    {
      m = ch == '-';
      ch = read_input ();
    }

  if (ch == ':')
    {
      *errp = INPUT_FORMAT;
      return 0;
    }

  unsigned long long v = 0;
  while (1)
    {
      unsigned long long v0 = v;

      if (isdigit (ch))
	{
	  v = 10 * v + (ch - '0');
	  if (!(TYPE_T_UNSIGNED_P
		|| (v & ~((~0ULL) >> 1)) == 0
		|| (m && v == ~((~0ULL) >> 1)))
	      || v < v0)
	    {
	      *errp = INPUT_OVERFLOW;
	      return 0;
	    }
	}
      else if (ch < 0)
	{
	  *errp = ch;
	  return 0;
	}
      else if (ch != ':')
	{
	  *errp = INPUT_FORMAT;
	  return 0;
	}
      else
	break;

      ch = read_input ();
    }

  *errp = 0;
  return m ? -v : v;
}

/* Return an error message corresponding to ERR.  */

static const char *
get_error_message (int err)
{
  switch (err)
    {
    case INPUT_EOF:
      return "input line %zi: premature end of input";
    case INPUT_ERROR:
      return "input line %zi: error reading input data: %m";
    case INPUT_FORMAT:
      return "input line %zi: input data format error";
    case INPUT_OVERFLOW:
      return "input line %zi: input data arithmetic overflow";
    case OUTPUT_TERM:
      return "input line %zi: string termination missing from output";
    case OUTPUT_OVERRUN:
      return "input line %zi: output data overrun";
    default:
      return "input line %zi: internal test error";
    }
}

/* Consume a record supplied by READ_INPUT above, according to '%' and
   any assignment-suppressing character '*', followed by any width W,
   any length modifier L, and conversion C, all already provided in FMT
   (along with trailing "%lln" implicitly appended by the caller) and
   removed from input along with the following ':' field separator.
   For convenience the last character of conversion C is supplied as
   the F parameter.

   Record formats consumed:

   %*<L><C>:<INPUT>:<RESULT==0>:<COUNT==-1>:
   %*<W><L><C>:<INPUT>:<RESULT==0>:<COUNT==-1>:
   %<L><C>:<INPUT>:<RESULT==0>:<COUNT==-1>:
   %<W><L><C>:<INPUT>:<RESULT==0>:<COUNT==-1>:
   %*<L><C>:<INPUT>:<RESULT>:<COUNT>:
   %*<W><L><C>:<INPUT>:<RESULT>:<COUNT>:
   %<L><C>:<INPUT>:<RESULT!=0>:<COUNT>:<MATCH>:
   %<W><L><C>:<INPUT>:<RESULT!=0>:<COUNT>:<MATCH>:

   Verify that the 'scanf' family function under test returned RESULT,
   that the "%lln" conversion recorded COUNT characters or has not been
   executed leaving the value at -1 as applicable, and where executed
   that the conversion requested produced output matching MATCH.

   Return 0 on success, -1 on failure.  */

static int
do_scanf (char f, char *fmt)
{
  bool value_match = true;
  bool count_match = true;
  long long count = -1;
  bool match = true;
  long long result;
  long long r;
  long long c;
  type_t val;
  int err;
  int ch;

  initialize_value (val);
  /* Make sure it's been committed.  */
  __asm__ ("" : : : "memory");

  if (fmt[1] == '*')
    result = scanf_under_test (fmt, &count);
  else
    result = scanf_under_test (fmt, pointer_to_value (val), &count);
  if (result < 0)
    FAIL_RET (get_error_message (result), line);

  do
    ch = read_input ();
  while (ch != ':' && ch != INPUT_ERROR && ch != INPUT_EOF);
  if (ch != ':')
    FAIL_RET (get_error_message (ch), line);

  r = read_integer (&err);
  if (err < 0)
    FAIL_RET (get_error_message (err), line);
  match &= r == result;

  c = read_integer (&err);
  if (err < 0)
    FAIL_RET (get_error_message (err), line);
  match &= (count_match = c == count);

  if (r > 0)
    {
      match &= (value_match = verify_input (f, val, count, &err));
      if (err < 0)
	FAIL_RET (get_error_message (err), line);
    }

  ch = read_input ();
  if (ch != '\n')
    FAIL_RET (get_error_message (ch == INPUT_ERROR || ch == INPUT_EOF
				 ? ch : INPUT_FORMAT), line);

  if (!match)
    {
      if (r != result)
	FAIL ("input line %zi: input assignment count mismatch: %lli",
	      line, result);
      if (!count_match)
	FAIL ("input line %zi: input character count mismatch: %lli",
	      line, count);
      if (!value_match)
	FAIL ("input line %zi: input value mismatch", line);
      return -1;
    }

  return 0;
}

/* Consume a list of input records line by line supplied by READ_INPUT
   above, discarding any that begin with the '#' line comment designator
   and interpreting the initial part of the remaining ones from leading
   '%' up to the first ':' field separator, which is removed from input,
   by appending "%lln" to the part retrieved and handing over along with
   the rest of input line to read to DO_SCANF above.  Terminate upon the
   end of input or the first processing error encountered.

   See the top of this file for the definitions that have to be
   provided by the source including this skeleton.  */

int
do_test (void)
{
  size_t fmt_size = 0;
  char *fmt = NULL;

  mtrace ();

  int result = 0;
  do
    {
      size_t i = 0;
      int ch = 0;
      char f;

      line++;
      do
	{
	  f = ch;
	  ch = read_input ();
	  if ((i == 0 && ch == '#') || ch == INPUT_EOF || ch == INPUT_ERROR)
	    break;
	  if (i == fmt_size)
	    {
	      fmt_size += SIZE_CHUNK;
	      fmt = xrealloc (fmt, fmt_size);
	    }
	  fmt[i++] = ch;
	}
      while (ch != ':');
      if (ch == INPUT_EOF && i == 0)
	{
	  if (line == 1)
	    {
	      FAIL ("input line %zi: empty input", line);
	      result = -1;
	    }
	  break;
	}
      if (ch == INPUT_ERROR)
	{
	  FAIL ("input line %zi: error reading format string: %m", line);
	  result = -1;
	  break;
	}
      if (ch == '#')
	{
	  do
	    ch = read_input ();
	  while (ch != '\n' && ch != INPUT_EOF && ch != INPUT_ERROR);
	  if (ch == '\n')
	    continue;

	  if (ch == INPUT_EOF)
	    FAIL ("input line %zi: premature end of input reading comment",
		  line);
	  else
	    FAIL ("input line %zi: error reading comment: %m", line);
	  result = -1;
	  break;
	}
      if (ch != ':' || i < 3 || fmt[0] != '%')
	{
	  FAIL ("input line %zi: format string format error: \"%.*s\"", line,
		(int) (i - 1), fmt);
	  result = -1;
	  break;
	}

      if (i + 4 > fmt_size)
	{
	  fmt_size += SIZE_CHUNK;
	  fmt = xrealloc (fmt, fmt_size);
	}
      fmt[i - 1] = '%';
      fmt[i++] = 'l';
      fmt[i++] = 'l';
      fmt[i++] = 'n';
      fmt[i++] = '\0';

      result = do_scanf (f, fmt);
    }
  while (result == 0);

  free (fmt);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Interpose 'dladdr' with a stub to speed up malloc tracing.  */

int
dladdr (const void *addr, Dl_info *info)
{
  return 0;
}

#include <support/test-driver.c>
