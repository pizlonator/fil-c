/* Searching a string for a character.
   Copyright (C) 2007-2024 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2007.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <string.h>

#include <stdlib.h>

#if GNULIB_MCEL_PREFER
# include "mcel.h"
#else
# include "mbuiterf.h"
#endif

/* Locate the first single-byte character C in the character string STRING,
   and return a pointer to it.  Return NULL if C is not found in STRING.  */
char *
mbschr (const char *string, int c)
{
  if (MB_CUR_MAX > 1
      /* Optimization: We know that ASCII characters < 0x30 don't occur as
         part of multibyte characters longer than 1 byte.  Hence, if c < 0x30,
         the faster unibyte loop can be used.  */
      && (unsigned char) c >= 0x30)
    {
#if GNULIB_MCEL_PREFER
      while (*string)
        {
          mcel_t g = mcel_scanz (string);
          if (g.len == 1 && (unsigned char) *string == (unsigned char) c)
            return (char *) string;
          string += g.len;
        }
#else
      mbuif_state_t state;
      const char *iter;
      for (mbuif_init (state), iter = string;; )
        {
          if (!mbuif_avail (state, iter))
            goto notfound;
          mbchar_t cur = mbuif_next (state, iter);
          if (mb_len (cur) == 1 && (unsigned char) *iter == (unsigned char) c)
            break;
          iter += mb_len (cur);
        }
      return (char *) iter;
     notfound:
#endif
      return NULL;
    }
  else
    return strchr (string, c);
}
