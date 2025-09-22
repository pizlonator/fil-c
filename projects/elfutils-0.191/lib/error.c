/* Definitions for error fallback functions.
   Copyright (C) 2021 Google, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

#if !defined(HAVE_ERROR_H) && defined(HAVE_ERR_H)
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

unsigned int error_message_count = 0;

void error(int status, int errnum, const char *format, ...) {
  va_list argp;
  int saved_errno = errno;

  fflush (stdout);

  va_start(argp, format);
  if (status)
    {
      if (errnum)
        {
          errno = errnum;
          verr (status, format, argp);
        }
      else
        verrx (status, format, argp);
    }
  else
    {
      if (errnum)
        {
          errno = errnum;
          vwarn (format, argp);
        }
      else
        vwarnx (format, argp);
    }
  va_end(argp);

  fflush (stderr);

  ++error_message_count;

  errno = saved_errno;
}
#endif
