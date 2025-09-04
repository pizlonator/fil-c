/* Parse dates for touch and date.

   Copyright (C) 1989-1991, 1998, 2000-2024 Free Software Foundation, Inc.

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

/* Yacc-based version written by Jim Kingdon and David MacKenzie.
   Rewritten by Jim Meyering.  */

#include <config.h>

#include "posixtm.h"

#include "c-ctype.h"
#include "idx.h"
#include "verify.h"

#include <stdckdint.h>
#include <string.h>

/*
  POSIX requires:

  touch -t [[CC]YY]mmddhhmm[.ss] FILE...
    8, 10, or 12 digits, followed by optional .ss
    (PDS_CENTURY | PDS_SECONDS)

  touch mmddhhmm[YY] FILE... (obsoleted by POSIX 1003.1-2001)
    8 or 10 digits, YY (if present) must be in the range 69-99
    (PDS_TRAILING_YEAR | PDS_PRE_2000)

  date mmddhhmm[[CC]YY]
    8, 10, or 12 digits
    (PDS_TRAILING_YEAR | PDS_CENTURY)

*/

static bool
year (struct tm *tm, const int *digit_pair, idx_t n, unsigned int syntax_bits)
{
  switch (n)
    {
    case 1:
      tm->tm_year = *digit_pair;
      /* Deduce the century based on the year.
         POSIX requires that 00-68 be interpreted as 2000-2068,
         and that 69-99 be interpreted as 1969-1999.  */
      if (digit_pair[0] <= 68)
        {
          if (syntax_bits & PDS_PRE_2000)
            return false;
          tm->tm_year += 100;
        }
      break;

    case 2:
      if (! (syntax_bits & PDS_CENTURY))
        return false;
      tm->tm_year = digit_pair[0] * 100 + digit_pair[1] - 1900;
      break;

    case 0:
      {
        /* Use current year.  */
        time_t now = time (NULL);
        struct tm *tmp = localtime (&now);
        if (! tmp)
          return false;
        tm->tm_year = tmp->tm_year;
      }
      break;

    default:
      assume (false);
    }

  return true;
}

static bool
posix_time_parse (struct tm *tm, const char *s, unsigned int syntax_bits)
{
  const char *dot = NULL;
  int pair[6];

  idx_t s_len = strlen (s);
  idx_t len = s_len;

  if (syntax_bits & PDS_SECONDS)
    {
      dot = strchr (s, '.');
      if (dot)
        {
          len = dot - s;
          if (s_len - len != 3)
            return false;
        }
    }

  if (! (8 <= len && len <= 12 && len % 2 == 0))
    return false;

  for (idx_t i = 0; i < len; i++)
    if (!c_isdigit (s[i]))
      return false;

  len /= 2;
  for (idx_t i = 0; i < len; i++)
    pair[i] = 10 * (s[2*i] - '0') + s[2*i + 1] - '0';

  int *p = pair;
  if (! (syntax_bits & PDS_TRAILING_YEAR))
    {
      if (! year (tm, p, len - 4, syntax_bits))
        return false;
      p += len - 4;
      len = 4;
    }

  /* Handle 8 digits worth of 'MMDDhhmm'.  */
  tm->tm_mon = *p++ - 1;
  tm->tm_mday = *p++;
  tm->tm_hour = *p++;
  tm->tm_min = *p++;
  len -= 4;

  /* Handle any trailing year.  */
  if (syntax_bits & PDS_TRAILING_YEAR)
    {
      if (! year (tm, p, len, syntax_bits))
        return false;
    }

  /* Handle seconds.  */
  if (!dot)
    tm->tm_sec = 0;
  else if (c_isdigit (dot[1]) && c_isdigit (dot[2]))
    tm->tm_sec = 10 * (dot[1] - '0') + dot[2] - '0';
  else
    return false;

  return true;
}

/* Parse a POSIX-style date, returning true if successful.  */

bool
posixtime (time_t *p, const char *s, unsigned int syntax_bits)
{
  struct tm tm0;
  bool leapsec = false;

  if (! posix_time_parse (&tm0, s, syntax_bits))
    return false;

  while (true)
    {
      struct tm tm1;
      tm1.tm_sec = tm0.tm_sec;
      tm1.tm_min = tm0.tm_min;
      tm1.tm_hour = tm0.tm_hour;
      tm1.tm_mday = tm0.tm_mday;
      tm1.tm_mon = tm0.tm_mon;
      tm1.tm_year = tm0.tm_year;
      tm1.tm_wday = -1;
      tm1.tm_isdst = -1;
      time_t t = mktime (&tm1);

      if (tm1.tm_wday < 0)
        return false;

      /* Reject dates like "September 31" and times like "25:61".
         However, allow a seconds count of 60 even in time zones that do
         not support leap seconds, treating it as the following second;
         POSIX requires this.  */
      if (! ((tm0.tm_year ^ tm1.tm_year)
             | (tm0.tm_mon ^ tm1.tm_mon)
             | (tm0.tm_mday ^ tm1.tm_mday)
             | (tm0.tm_hour ^ tm1.tm_hour)
             | (tm0.tm_min ^ tm1.tm_min)
             | (tm0.tm_sec ^ tm1.tm_sec)))
        {
          if (ckd_add (&t, t, +leapsec))
            return false;
          *p = t;
          return true;
        }

      /* Any mismatch without 60 in the tm_sec field is invalid.  */
      if (tm0.tm_sec != 60)
        return false;

      /* Allow times like 01:35:60 or 23:59:60.  */
      tm0.tm_sec = 59;
      leapsec = true;
    }
}
