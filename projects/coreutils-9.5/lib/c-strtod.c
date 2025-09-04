/* Convert string to floating-point number, using the C locale.

   Copyright (C) 2003-2004, 2006, 2009-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert and Bruno Haible.  */

#include <config.h>

#include "c-strtod.h"

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#if FLOAT
# define C_STRTOD c_strtof
# define DOUBLE float
# define STRTOD_L strtof_l
# define HAVE_GOOD_STRTOD_L (HAVE_STRTOF_L && !GNULIB_defined_strtof_function)
# define STRTOD strtof
#elif LONG
# define C_STRTOD c_strtold
# define DOUBLE long double
# define STRTOD_L strtold_l
# define HAVE_GOOD_STRTOD_L (HAVE_STRTOLD_L && !GNULIB_defined_strtold_function)
# define STRTOD strtold
#else
# define C_STRTOD c_strtod
# define DOUBLE double
# define STRTOD_L strtod_l
# define HAVE_GOOD_STRTOD_L (HAVE_STRTOD_L && !GNULIB_defined_strtod_function)
# define STRTOD strtod
#endif

#if defined LC_ALL_MASK && (HAVE_GOOD_STRTOD_L || HAVE_WORKING_USELOCALE)

/* Cache for the C locale object.
   Marked volatile so that different threads see the same value
   (avoids locking).  */
static volatile locale_t c_locale_cache;

/* Return the C locale object, or (locale_t) 0 with errno set
   if it cannot be created.  */
static locale_t
c_locale (void)
{
  if (!c_locale_cache)
    c_locale_cache = newlocale (LC_ALL_MASK, "C", (locale_t) 0);
  return c_locale_cache;
}

#else

# if HAVE_NL_LANGINFO
#  include <langinfo.h>
# endif
# include "c-ctype.h"

/* Determine the decimal-point character according to the current locale.  */
static char
decimal_point_char (void)
{
  const char *point;
  /* Determine it in a multithread-safe way.  We know nl_langinfo is
     multithread-safe on glibc systems and Mac OS X systems, but is not required
     to be multithread-safe by POSIX.  sprintf(), however, is multithread-safe.
     localeconv() is rarely multithread-safe.  */
# if HAVE_NL_LANGINFO && (__GLIBC__ || defined __UCLIBC__ || (defined __APPLE__ && defined __MACH__))
  point = nl_langinfo (RADIXCHAR);
# elif 1
  char pointbuf[5];
  sprintf (pointbuf, "%#.0f", 1.0);
  point = &pointbuf[1];
# else
  point = localeconv () -> decimal_point;
# endif
  /* The decimal point is always a single byte: either '.' or ','.  */
  return (point[0] != '\0' ? point[0] : '.');
}

#endif

DOUBLE
C_STRTOD (char const *nptr, char **endptr)
{
  DOUBLE r;

#if defined LC_ALL_MASK && (HAVE_GOOD_STRTOD_L || HAVE_WORKING_USELOCALE)

  locale_t locale = c_locale ();
  if (!locale)
    {
      if (endptr)
        *endptr = (char *) nptr;
      return 0; /* errno is set here */
    }

# if HAVE_GOOD_STRTOD_L

  r = STRTOD_L (nptr, endptr, locale);

# else /* HAVE_WORKING_USELOCALE */

  locale_t old_locale = uselocale (locale);
  if (old_locale == (locale_t)0)
    {
      if (endptr)
        *endptr = (char *) nptr;
      return 0; /* errno is set here */
    }

  r = STRTOD (nptr, endptr);

  int saved_errno = errno;
  if (uselocale (old_locale) == (locale_t)0)
    /* We can't switch back to the old locale.  The thread is hosed.  */
    abort ();
  errno = saved_errno;

# endif

#else

  char decimal_point = decimal_point_char ();

  if (decimal_point == '.')
    /* In this locale, C_STRTOD and STRTOD behave the same way.  */
    r = STRTOD (nptr, endptr);
  else
    {
      /* Start and end of the floating-point number.  */
      char const *start;
      char const *end;
      /* Position of the decimal point '.' in the floating-point number.
         Either decimal_point_p == NULL or start <= decimal_point_p < end.  */
      char const *decimal_point_p = NULL;
      /* Set to true if we encountered decimal_point while parsing.  */
      int seen_decimal_point = 0;

      /* Parse
         1. a sequence of white-space characters,
         2. a subject sequence possibly containing a floating-point number,
         as described in POSIX
         <https://pubs.opengroup.org/onlinepubs/9699919799/functions/strtod.html>.
       */
      {
        char const *p;

        p = nptr;

        /* Parse a sequence of white-space characters.  */
        while (*p != '\0' && c_isspace ((unsigned char) *p))
          p++;
        start = p;
        end = p;

        /* Start to parse a subject sequence.  */
        if (*p == '+' || *p == '-')
          p++;
        if (*p == '0')
          {
            end = p + 1;
            if (p[1] == 'x' || p[1] == 'X')
              {
                size_t num_hex_digits = 0;
                p += 2;
                /* Parse a non-empty sequence of hexadecimal digits optionally
                   containing the decimal point character '.'.  */
                while (*p != '\0')
                  {
                    if (c_isxdigit ((unsigned char) *p))
                      {
                        num_hex_digits++;
                        p++;
                      }
                    else if (*p == '.')
                      {
                        if (decimal_point_p == NULL)
                          {
                            decimal_point_p = p;
                            p++;
                          }
                        else
                          break;
                      }
                    else
                      break;
                  }
                seen_decimal_point = (*p == decimal_point);
                if (num_hex_digits > 0)
                  {
                    end = p;
                    /* Parse the character 'p' or the character 'P', optionally
                       followed by a '+' or '-' character, and then followed by
                       one or more decimal digits.  */
                    if (*p == 'p' || *p == 'P')
                      {
                        p++;
                        if (*p == '+' || *p == '-')
                          p++;
                        if (*p != '\0' && c_isdigit ((unsigned char) *p))
                          {
                            p++;
                            while (*p != '\0' && c_isdigit ((unsigned char) *p))
                              p++;
                            end = p;
                          }
                      }
                  }
                else
                  decimal_point_p = NULL;
                goto done_parsing;
              }
          }
        {
          size_t num_digits = 0;
          /* Parse a non-empty sequence of decimal digits optionally containing
             the decimal point character '.'.  */
          while (*p != '\0')
            {
              if (c_isdigit ((unsigned char) *p))
                {
                  num_digits++;
                  p++;
                }
              else if (*p == '.')
                {
                  if (decimal_point_p == NULL)
                    {
                      decimal_point_p = p;
                      p++;
                    }
                  else
                    break;
                }
              else
                break;
            }
          seen_decimal_point = (*p == decimal_point);
          if (num_digits > 0)
            {
              end = p;
              /* Parse the character 'e' or the character 'E', optionally
                 followed by a '+' or '-' character, and then followed by one or
                 more decimal digits.  */
              if (*p == 'e' || *p == 'E')
                {
                  p++;
                  if (*p == '+' || *p == '-')
                    p++;
                  if (*p != '\0' && c_isdigit ((unsigned char) *p))
                    {
                      p++;
                      while (*p != '\0' && c_isdigit ((unsigned char) *p))
                        p++;
                      end = p;
                    }
                }
            }
          else
            decimal_point_p = NULL;
        }
      }
     done_parsing:
      if (end == start || (decimal_point_p == NULL && !seen_decimal_point))
        /* If end == start, we have not parsed anything.  The given string
           might be "INF", "INFINITY", "NAN", "NAN(chars)", or invalid.
           We can pass it to STRTOD.
           If end > start and decimal_point_p == NULL, we have parsed a
           floating-point number, and it does not have a '.' (and not a ','
           either, of course).  If !seen_decimal_point, we did not
           encounter decimal_point while parsing.  In this case, the
           locale-dependent STRTOD function can handle it.  */
        r = STRTOD (nptr, endptr);
      else
        {
          /* Create a modified floating-point number, in which the character '.'
             is replaced with the locale-dependent decimal_point.  */
          size_t len = end - start;
          char *buf;
          char *buf_malloced = NULL;
          char stackbuf[1000];
          char *end_in_buf;

          if (len < sizeof (stackbuf))
            buf = stackbuf;
          else
            {
              buf = malloc (len + 1);
              if (buf == NULL)
                {
                  /* Out of memory.
                     Callers may not be prepared to see errno == ENOMEM.  But
                     they should be prepared to see errno == EINVAL.  */
                  errno = EINVAL;
                  if (endptr != NULL)
                    *endptr = (char *) nptr;
                  return 0;
                }
              buf_malloced = buf;
            }

          memcpy (buf, start, len);
          if (decimal_point_p != NULL)
            buf[decimal_point_p - start] = decimal_point;
          buf[len] = '\0';

          r = STRTOD (buf, &end_in_buf);
          if (endptr != NULL)
            *endptr = (char *) (start + (end_in_buf - buf));

          if (buf_malloced != NULL)
            {
              int saved_errno = errno;
              free (buf_malloced);
              errno = saved_errno;
            }
        }
    }

#endif

  return r;
}
