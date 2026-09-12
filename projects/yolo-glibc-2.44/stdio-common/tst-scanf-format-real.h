/* Test feature wrapper for formatted real input.
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

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <tgmath.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <support/support.h>

/* Reference data is a case-inensitive signed datum, which is either a
   hexadecimal floating constant or a literal representing infinity or
   NaN data in any of the forms accepted by 'strtod' family functions,
   whose binary representation is to match against byte-wise.

   We need to be careful with parsing reference data in the handling of
   the sign as with the IBM long double binary format unary negation
   of a positive value whose lower-magnitude part is zero produces a
   bit pattern that is different from one produced by initialization or
   conversions made by functions such as 'scanf' or 'strtod' from the
   complementing negative value.  The difference is in the sign of zero
   held in the lower-magnitude part and therefore the bit patterns are
   arithmetically equivalent, but they do not match byte-wise.

   Therefore we set the sign with the initial value of the significand,
   either -0.0 or 0.0, which sets the sign of both parts according to
   what 'scanf' does, and never negate it afterwards.

   Additionally the Intel long double binary format uses only 80 bits
   out of 96 that the data type occupies.  Therefore preinitialize the
   reference value with the same 0xa5 bit pattern that the value under
   test has been so that the byte-wise comparison matches as well for
   the unused parts of the two data pieces.

   We use 'ldexp' to assemble the significand with the exponent, which
   does not compromise verification, because internally this exercises
   a code path different from one used by 'scanf' family functions for
   this purpose.  Specifically 'ldexp' uses 'scalbn', whereas 'scanf'
   defers to 'strtod' which uses '__mpn_construct_double'.  Analogously
   for the remaining floating-point data types.  */

#define nan(v, x)							\
  _Generic((v), float: nanf, double: nan, long double: nanl) (x)

#define pointer_to_value(val) (&(val))

#define initialize_value(val)						\
  memset (&val, 0xa5, sizeof (val))

#ifndef compare_real
static bool
compare_real (type_t x, type_t y)
{
  return memcmp (&x, &y, sizeof (y)) == 0;
}
#endif

static type_t
read_real (int *errp)
{
  bool m = false;
  int err = 0;
  type_t v;
  int ch;

  ch = read_input ();
  if (ch == '-' || ch == '+')
    {
      m = ch == '-';
      ch = read_input ();
    }

  switch (ch)
    {
    case '0':
      break;
    case 'I':
    case 'i':
      {
	static const char unf[] = { 'N', 'F' };
	static const char lnf[] = { 'n', 'f' };
	size_t i;

	for (i = 0; i < sizeof (unf); i++)
	  {
	    ch = read_input ();
	    if (ch != unf[i] && ch != lnf[i])
	      {
		err = ch < 0 ? ch : INPUT_FORMAT;
		v = NAN;
		goto out;
	      }
	  }

	ch = read_input ();
	if (ch == ':')
	  {
	    v = m ? -INFINITY : +INFINITY;
	    goto out;
	  }

	static const char uinity[] = { 'I', 'N', 'I', 'T', 'Y' };
	static const char linity[] = { 'i', 'n', 'i', 't', 'y' };

	for (i = 0; i < sizeof (uinity); i++)
	  {
	    if (ch != uinity[i] && ch != linity[i])
	      {
		err = ch < 0 ? ch : INPUT_FORMAT;
		v = NAN;
		goto out;
	      }
	    ch = read_input ();
	  }
	if (ch == ':')
	  {
	    v = m ? -INFINITY : +INFINITY;
	    goto out;
	  }
      }
      err = ch < 0 ? ch : INPUT_FORMAT;
      v = NAN;
      goto out;

    case 'N':
    case 'n':
      {
	static const char uan[] = { 'A', 'N' };
	static const char lan[] = { 'a', 'n' };
	size_t i;

	for (i = 0; i < sizeof (uan); i++)
	  {
	    ch = read_input ();
	    if (ch != uan[i] && ch != lan[i])
	      {
		err = ch < 0 ? ch : INPUT_FORMAT;
		v = NAN;
		goto out;
	      }
	  }

	ch = read_input ();
	if (ch == ':')
	  {
	    v = m ? -nan (v, ".") : nan (v, ".");
	    goto out;
	  }

	if (ch == '(')
	  {
	    size_t seq_size = 0;
	    char *seq = NULL;
	    i = 0;
	    while (1)
	      {
		if (i == seq_size)
		  {
		    seq_size += SIZE_CHUNK;
		    seq = xrealloc (seq, seq_size);
		  }
		ch = read_input ();
		if (ch == ')')
		  break;
		if (ch != '_' && !isdigit (ch)
		    && !(ch >= 'A' && ch <= 'Z')
		    && !(ch >= 'a' && ch <= 'z'))
		  {
		    free (seq);
		    err = ch < 0 ? ch : INPUT_FORMAT;
		    v = NAN;
		    goto out;
		  }
		seq[i++] = ch;
	      }
	    seq[i] = '\0';

	    ch = read_input ();
	    if (ch == ':')
	      {
		v = m ? -nan (v, seq) : nan (v, seq);
		free (seq);
		goto out;
	      }
	    free (seq);
	  }
      }
      err = ch < 0 ? ch : INPUT_FORMAT;
      v = NAN;
      goto out;

    default:
      err = ch < 0 ? ch : INPUT_FORMAT;
      v = NAN;
      goto out;
    }

  ch = read_input ();
  if (ch != 'X' && ch != 'x')
    {
      err = ch < 0 ? ch : INPUT_FORMAT;
      v = NAN;
      goto out;
    }

  type_t f = m ? -1.0 : 1.0;
  v = m ? -0.0 : 0.0;
  int i = 0;
  do
    {
      int d = 0;

      ch = read_input ();

      if (i == 1)
	switch (ch)
	  {
	  case '.':
	    i++;
	    continue;

	  case ':':
	  case 'P':
	  case 'p':
	    break;

	  default:
	    err = ch < 0 ? ch : INPUT_FORMAT;
	    v = NAN;
	    goto out;
	  }

      switch (ch)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  d = ch - '0';
	  break;

	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	  d = ch - 'A' + 10;
	  break;

	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	  d = ch - 'a' + 10;
	  break;

	case ':':
	case 'P':
	case 'p':
	  if (i == 0)
	    {
	      err = INPUT_FORMAT;
	      v = NAN;
	      goto out;
	    }
	  break;

	default:
	  err = ch < 0 ? ch : INPUT_FORMAT;
	  v = NAN;
	  goto out;
	}

      v += f * d;
      f /= 16.0l;
      i++;
    }
  while (ch != ':' && ch != 'P' && ch != 'p');

  long long exp = 0;
  if (ch == 'P' || ch == 'p')
    {
      exp = read_integer (&err);
      if (err)
	{
	  v = NAN;
	  goto out;
	}
    }

  errno = 0;
  v = ldexp (v, exp);
  if ((v == HUGE_VALL || v == -HUGE_VALL) && errno != 0)
    {
      err = INPUT_OVERFLOW;
      v = NAN;
      goto out;
    }

out:
  *errp = err;
  return v;
}

static bool
verify_input (char f, type_t val, long long count, int *errp)
{
  bool match = true;
  int err = 0;
  type_t v;

  initialize_value (v);
  /* Make sure it's been committed.  */
  __asm__ ("" : : : "memory");

  v = read_real (&err);
  if (err < 0)
    goto out;

  match = compare_real (val, v);
  if (!match)
    {
      union
	{
	  type_t v;
	  unsigned char x[sizeof (type_t)];
	}
      uv = { .v = v }, ui = { .v = val };

      printf ("error: %s:%d: input buffer: `", __FILE__, __LINE__);
      for (size_t j = 0; j < sizeof (ui.x); j++)
	printf ("%02hhx", ui.x[j]);
      printf ("'\n");
      printf ("error: %s:%d: value buffer: `", __FILE__, __LINE__);
      for (size_t j = 0; j < sizeof (uv.x); j++)
	printf ("%02hhx", uv.x[j]);
      printf ("'\n");
    }

out:
  *errp = err;
  return match;
}
