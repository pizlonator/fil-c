/* Copyright (C) 1991-1992, 1997, 1999, 2003, 2006, 2008-2021 Free Software
   Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#if ! defined USE_LONG_DOUBLE
# include <config.h>
#endif

/* Specification.  */
#include <stdlib.h>

#include <ctype.h>      /* isspace() */
#include <errno.h>
#include <float.h>      /* {DBL,LDBL}_{MIN,MAX} */
#include <limits.h>     /* LONG_{MIN,MAX} */
#include <locale.h>     /* localeconv() */
#include <math.h>       /* NAN */
#include <stdbool.h>
#include <stdio.h>      /* sprintf() */
#include <string.h>     /* strdup() */
#if HAVE_NL_LANGINFO
# include <langinfo.h>
#endif

#include "c-ctype.h"

#undef MIN
#undef MAX
#ifdef USE_LONG_DOUBLE
# define STRTOD strtold
# define LDEXP ldexpl
# if defined __hpux && defined __hppa
   /* We cannot call strtold on HP-UX/hppa, because its return type is a struct,
      not a 'long double'.  */
#  define HAVE_UNDERLYING_STRTOD 0
# elif STRTOLD_HAS_UNDERFLOW_BUG
   /* strtold would not set errno=ERANGE upon underflow.  */
#  define HAVE_UNDERLYING_STRTOD 0
# else
#  define HAVE_UNDERLYING_STRTOD HAVE_STRTOLD
# endif
# define DOUBLE long double
# define MIN LDBL_MIN
# define MAX LDBL_MAX
# define L_(literal) literal##L
#else
# define STRTOD strtod
# define LDEXP ldexp
# define HAVE_UNDERLYING_STRTOD 1
# define DOUBLE double
# define MIN DBL_MIN
# define MAX DBL_MAX
# define L_(literal) literal
#endif

#if (defined USE_LONG_DOUBLE ? HAVE_LDEXPM_IN_LIBC : HAVE_LDEXP_IN_LIBC)
# define USE_LDEXP 1
#else
# define USE_LDEXP 0
#endif

/* Return true if C is a space in the current locale, avoiding
   problems with signed char and isspace.  */
static bool
locale_isspace (char c)
{
  unsigned char uc = c;
  return isspace (uc) != 0;
}

/* Determine the decimal-point character according to the current locale.  */
static char
decimal_point_char (void)
{
  const char *point;
  /* Determine it in a multithread-safe way.  We know nl_langinfo is
     multithread-safe on glibc systems and Mac OS X systems, but is not required
     to be multithread-safe by POSIX.  sprintf(), however, is multithread-safe.
     localeconv() is rarely multithread-safe.  */
#if HAVE_NL_LANGINFO && (__GLIBC__ || defined __UCLIBC__ || (defined __APPLE__ && defined __MACH__))
  point = nl_langinfo (RADIXCHAR);
#elif 1
  char pointbuf[5];
  sprintf (pointbuf, "%#.0f", 1.0);
  point = &pointbuf[1];
#else
  point = localeconv () -> decimal_point;
#endif
  /* The decimal point is always a single byte: either '.' or ','.  */
  return (point[0] != '\0' ? point[0] : '.');
}

#if !USE_LDEXP
 #undef LDEXP
 #define LDEXP dummy_ldexp
 /* A dummy definition that will never be invoked.  */
 static DOUBLE LDEXP (_GL_UNUSED DOUBLE x, _GL_UNUSED int exponent)
 {
   abort ();
   return L_(0.0);
 }
#endif

/* Return X * BASE**EXPONENT.  Return an extreme value and set errno
   to ERANGE if underflow or overflow occurs.  */
static DOUBLE
scale_radix_exp (DOUBLE x, int radix, long int exponent)
{
  /* If RADIX == 10, this code is neither precise nor fast; it is
     merely a straightforward and relatively portable approximation.
     If N == 2, this code is precise on a radix-2 implementation,
     albeit perhaps not fast if ldexp is not in libc.  */

  long int e = exponent;

  if (USE_LDEXP && radix == 2)
    return LDEXP (x, e < INT_MIN ? INT_MIN : INT_MAX < e ? INT_MAX : e);
  else
    {
      DOUBLE r = x;

      if (r != 0)
        {
          if (e < 0)
            {
              while (e++ != 0)
                {
                  r /= radix;
                  if (r == 0 && x != 0)
                    {
                      errno = ERANGE;
                      break;
                    }
                }
            }
          else
            {
              while (e-- != 0)
                {
                  if (r < -MAX / radix)
                    {
                      errno = ERANGE;
                      return -HUGE_VAL;
                    }
                  else if (MAX / radix < r)
                    {
                      errno = ERANGE;
                      return HUGE_VAL;
                    }
                  else
                    r *= radix;
                }
            }
        }

      return r;
    }
}

/* Parse a number at NPTR; this is a bit like strtol (NPTR, ENDPTR)
   except there are no leading spaces or signs or "0x", and ENDPTR is
   nonnull.  The number uses a base BASE (either 10 or 16) fraction, a
   radix RADIX (either 10 or 2) exponent, and exponent character
   EXPCHAR.  BASE is RADIX**RADIX_MULTIPLIER.  */
static DOUBLE
parse_number (const char *nptr,
              int base, int radix, int radix_multiplier, char radixchar,
              char expchar,
              char **endptr)
{
  const char *s = nptr;
  const char *digits_start;
  const char *digits_end;
  const char *radixchar_ptr;
  long int exponent;
  DOUBLE num;

  /* First, determine the start and end of the digit sequence.  */
  digits_start = s;
  radixchar_ptr = NULL;
  for (;; ++s)
    {
      if (base == 16 ? c_isxdigit (*s) : c_isdigit (*s))
        ;
      else if (radixchar_ptr == NULL && *s == radixchar)
        {
          /* Record that we have found the decimal point.  */
          radixchar_ptr = s;
        }
      else
        /* Any other character terminates the digit sequence.  */
        break;
    }
  digits_end = s;
  /* Now radixchar_ptr == NULL or
     digits_start <= radixchar_ptr < digits_end.  */

  if (false)
    { /* Unoptimized.  */
      exponent =
        (radixchar_ptr != NULL
         ? - (long int) (digits_end - radixchar_ptr - 1)
         : 0);
    }
  else
    { /* Remove trailing zero digits.  This reduces rounding errors for
         inputs such as 1.0000000000 or 10000000000e-10.  */
      while (digits_end > digits_start)
        {
          if (digits_end - 1 == radixchar_ptr || *(digits_end - 1) == '0')
            digits_end--;
          else
            break;
        }
      exponent =
        (radixchar_ptr != NULL
         ? (digits_end > radixchar_ptr
            ? - (long int) (digits_end - radixchar_ptr - 1)
            : (long int) (radixchar_ptr - digits_end))
         : (long int) (s - digits_end));
    }

  /* Then, convert the digit sequence to a number.  */
  {
    const char *dp;
    num = 0;
    for (dp = digits_start; dp < digits_end; dp++)
      if (dp != radixchar_ptr)
        {
          int digit;

          /* Make sure that multiplication by BASE will not overflow.  */
          if (!(num <= MAX / base))
            {
              /* The value of the digit and all subsequent digits don't matter,
                 since we have already gotten as many digits as can be
                 represented in a 'DOUBLE'.  This doesn't necessarily mean that
                 the result will overflow: The exponent may reduce it to within
                 range.  */
              exponent +=
                (digits_end - dp)
                - (radixchar_ptr >= dp && radixchar_ptr < digits_end ? 1 : 0);
              break;
            }

          /* Eat the next digit.  */
          if (c_isdigit (*dp))
            digit = *dp - '0';
          else if (base == 16 && c_isxdigit (*dp))
            digit = c_tolower (*dp) - ('a' - 10);
          else
            abort ();
          num = num * base + digit;
        }
  }

  exponent = exponent * radix_multiplier;

  /* Finally, parse the exponent.  */
  if (c_tolower (*s) == expchar && ! locale_isspace (s[1]))
    {
      /* Add any given exponent to the implicit one.  */
      int saved_errno = errno;
      char *end;
      long int value = strtol (s + 1, &end, 10);
      errno = saved_errno;

      if (s + 1 != end)
        {
          /* Skip past the exponent, and add in the implicit exponent,
             resulting in an extreme value on overflow.  */
          s = end;
          exponent =
            (exponent < 0
             ? (value < LONG_MIN - exponent ? LONG_MIN : exponent + value)
             : (LONG_MAX - exponent < value ? LONG_MAX : exponent + value));
        }
    }

  *endptr = (char *) s;
  return scale_radix_exp (num, radix, exponent);
}

/* HP cc on HP-UX 10.20 has a bug with the constant expression -0.0.
   ICC 10.0 has a bug when optimizing the expression -zero.
   The expression -MIN * MIN does not work when cross-compiling
   to PowerPC on Mac OS X 10.5.  */
static DOUBLE
minus_zero (void)
{
#if defined __hpux || defined __sgi || defined __ICC
  return -MIN * MIN;
#else
  return -0.0;
#endif
}

/* Convert NPTR to a DOUBLE.  If ENDPTR is not NULL, a pointer to the
   character after the last one used in the number is put in *ENDPTR.  */
DOUBLE
STRTOD (const char *nptr, char **endptr)
#if HAVE_UNDERLYING_STRTOD
# ifdef USE_LONG_DOUBLE
#  undef strtold
# else
#  undef strtod
# endif
#else
# undef STRTOD
# define STRTOD(NPTR,ENDPTR) \
   parse_number (NPTR, 10, 10, 1, radixchar, 'e', ENDPTR)
#endif
/* From here on, STRTOD refers to the underlying implementation.  It needs
   to handle only finite unsigned decimal numbers with non-null ENDPTR.  */
{
  char radixchar;
  bool negative = false;

  /* The number so far.  */
  DOUBLE num;

  const char *s = nptr;
  const char *end;
  char *endbuf;
  int saved_errno = errno;

  radixchar = decimal_point_char ();

  /* Eat whitespace.  */
  while (locale_isspace (*s))
    ++s;

  /* Get the sign.  */
  negative = *s == '-';
  if (*s == '-' || *s == '+')
    ++s;

  num = STRTOD (s, &endbuf);
  end = endbuf;

  if (c_isdigit (s[*s == radixchar]))
    {
      /* If a hex float was converted incorrectly, do it ourselves.
         If the string starts with "0x" but does not contain digits,
         consume the "0" ourselves.  If a hex float is followed by a
         'p' but no exponent, then adjust the end pointer.  */
      if (*s == '0' && c_tolower (s[1]) == 'x')
        {
          if (! c_isxdigit (s[2 + (s[2] == radixchar)]))
            {
              end = s + 1;

              /* strtod() on z/OS returns ERANGE for "0x".  */
              errno = saved_errno;
            }
          else if (end <= s + 2)
            {
              num = parse_number (s + 2, 16, 2, 4, radixchar, 'p', &endbuf);
              end = endbuf;
            }
          else
            {
              const char *p = s + 2;
              while (p < end && c_tolower (*p) != 'p')
                p++;
              if (p < end && ! c_isdigit (p[1 + (p[1] == '-' || p[1] == '+')]))
                {
                  char *dup = strdup (s);
                  errno = saved_errno;
                  if (!dup)
                    {
                      /* Not really our day, is it.  Rounding errors are
                         better than outright failure.  */
                      num =
                        parse_number (s + 2, 16, 2, 4, radixchar, 'p', &endbuf);
                    }
                  else
                    {
                      dup[p - s] = '\0';
                      num = STRTOD (dup, &endbuf);
                      saved_errno = errno;
                      free (dup);
                      errno = saved_errno;
                    }
                  end = p;
                }
            }
        }
      else
        {
          /* If "1e 1" was misparsed as 10.0 instead of 1.0, re-do the
             underlying STRTOD on a copy of the original string
             truncated to avoid the bug.  */
          const char *e = s + 1;
          while (e < end && c_tolower (*e) != 'e')
            e++;
          if (e < end && ! c_isdigit (e[1 + (e[1] == '-' || e[1] == '+')]))
            {
              char *dup = strdup (s);
              errno = saved_errno;
              if (!dup)
                {
                  /* Not really our day, is it.  Rounding errors are
                     better than outright failure.  */
                  num = parse_number (s, 10, 10, 1, radixchar, 'e', &endbuf);
                }
              else
                {
                  dup[e - s] = '\0';
                  num = STRTOD (dup, &endbuf);
                  saved_errno = errno;
                  free (dup);
                  errno = saved_errno;
                }
              end = e;
            }
        }

      s = end;
    }

  /* Check for infinities and NaNs.  */
  else if (c_tolower (*s) == 'i'
           && c_tolower (s[1]) == 'n'
           && c_tolower (s[2]) == 'f')
    {
      s += 3;
      if (c_tolower (*s) == 'i'
          && c_tolower (s[1]) == 'n'
          && c_tolower (s[2]) == 'i'
          && c_tolower (s[3]) == 't'
          && c_tolower (s[4]) == 'y')
        s += 5;
      num = HUGE_VAL;
      errno = saved_errno;
    }
  else if (c_tolower (*s) == 'n'
           && c_tolower (s[1]) == 'a'
           && c_tolower (s[2]) == 'n')
    {
      s += 3;
      if (*s == '(')
        {
          const char *p = s + 1;
          while (c_isalnum (*p))
            p++;
          if (*p == ')')
            s = p + 1;
        }

      /* If the underlying implementation misparsed the NaN, assume
         its result is incorrect, and return a NaN.  Normally it's
         better to use the underlying implementation's result, since a
         nice implementation populates the bits of the NaN according
         to interpreting n-char-sequence as a hexadecimal number.  */
      if (s != end || num == num)
        num = NAN;
      errno = saved_errno;
    }
  else
    {
      /* No conversion could be performed.  */
      errno = EINVAL;
      s = nptr;
    }

  if (endptr != NULL)
    *endptr = (char *) s;
  /* Special case -0.0, since at least ICC miscompiles negation.  We
     can't use copysign(), as that drags in -lm on some platforms.  */
  if (!num && negative)
    return minus_zero ();
  return negative ? -num : num;
}
