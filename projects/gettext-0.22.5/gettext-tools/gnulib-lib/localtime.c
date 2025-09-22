/* Work around platform bugs in localtime.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <time.h>

#include <stdlib.h>
#include <string.h>
#if defined _WIN32 && ! defined __CYGWIN__
# include <wchar.h>
#endif

#undef localtime

struct tm *
rpl_localtime (const time_t *tp)
{
#if defined _WIN32 && ! defined __CYGWIN__
  /* Rectify the value of the environment variable TZ.
     There are four possible kinds of such values:
       - Traditional US time zone names, e.g. "PST8PDT".  Syntax: see
         <https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/tzset>
       - Time zone names based on geography, that contain one or more
         slashes, e.g. "Europe/Moscow".
       - Time zone names based on geography, without slashes, e.g.
         "Singapore".
       - Time zone names that contain explicit DST rules.  Syntax: see
         <https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03>
     The Microsoft CRT understands only the first kind.  It produces incorrect
     results if the value of TZ is of the other kinds.
     But in a Cygwin environment, /etc/profile.d/tzset.sh sets TZ to a value
     of the second kind for most geographies, or of the first kind in a few
     other geographies.  If it is of the second kind, neutralize it.  For the
     Microsoft CRT, an absent or empty TZ means the time zone that the user
     has set in the Windows Control Panel.
     If the value of TZ is of the third or fourth kind -- Cygwin programs
     understand these syntaxes as well --, it does not matter whether we
     neutralize it or not, since these values occur only when a Cygwin user
     has set TZ explicitly; this case is 1. rare and 2. under the user's
     responsibility.  */
  const char *tz = getenv ("TZ");
  if (tz != NULL && strchr (tz, '/') != NULL)
    {
      /* Neutralize it, in a way that is multithread-safe.
         (If we were to use _putenv ("TZ="), it would free the memory allocated
         for the environment variable "TZ", and thus other threads that are
         using the previously fetched value of getenv ("TZ") could crash.)  */
      char **env = _environ;
      wchar_t **wenv = _wenviron;
      if (env != NULL)
        for (char *s = env; *s != NULL; s++)
          if (s[0] == 'T' && s[1] == 'Z' && s[2] == '=')
            s[0] = '$';
      if (wenv != NULL)
        for (wchar_t *ws = wenv; *ws != NULL; ws++)
          if (ws[0] == L'T' && ws[1] == L'Z' && ws[2] == L'=')
            ws[0] = L'$';
    }
#endif

  return localtime (tp);
}
