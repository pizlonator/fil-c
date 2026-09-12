/* Utilities functions to read/parse Linux procfs and sysfs.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

#include <assert.h>
#include <not-cancel.h>
#include <procutils.h>
#include <string.h>

static int
next_line (char **r, int fd, char *const buffer, char **cp, char **re,
           char *const buffer_end)
{
  char *res = *cp;
  char *nl = memchr (*cp, '\n', *re - *cp);
  if (nl == NULL)
    {
      if (*cp != buffer)
        {
          if (*re == buffer_end)
            {
              memmove (buffer, *cp, *re - *cp);
              *re = buffer + (*re - *cp);
              *cp = buffer;

              ssize_t n = TEMP_FAILURE_RETRY (
		__read_nocancel (fd, *re, buffer_end - *re));
              if (n < 0)
                return -1;

              *re += n;

              nl = memchr (*cp, '\n', *re - *cp);
	      if (nl == NULL)
	        /* Line too long.  */
		return 0;
            }
          else
            nl = memchr (*cp, '\n', *re - *cp);

          res = *cp;
        }

      if (nl == NULL)
        nl = *re - 1;
    }

  *nl = '\0';
  *cp = nl + 1;
  assert (*cp <= *re);

  if (res == *re)
    return 0;

  *r = res;
  return 1;
}

bool
__libc_procutils_read_file (const char *filename,
			    procutils_closure_t closure,
			    void *arg)
{
  enum { buffer_size = PROCUTILS_MAX_LINE_LEN };
  char buffer[buffer_size];
  char *buffer_end = buffer + buffer_size;
  char *cp = buffer_end;
  char *re = buffer_end;

  int fd = TEMP_FAILURE_RETRY (
    __open64_nocancel (filename, O_RDONLY | O_CLOEXEC));
  if (fd == -1)
    return false;

  char *l;
  int r;
  while ((r = next_line (&l, fd, buffer, &cp, &re, buffer_end)) > 0)
    if (closure (l, arg) != 0)
      break;

  __close_nocancel_nostatus (fd);

  return r == 1;
}
