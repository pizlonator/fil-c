/* readlink wrapper to return the link name in malloc'd storage.
   Unlike xreadlink and xreadlink_with_size, don't ever call exit.

   Copyright (C) 2001, 2003-2007, 2009-2024 Free Software Foundation, Inc.

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

/* Written by Jim Meyering <jim@meyering.net>  */

#include <config.h>

#include "areadlink.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* SYMLINK_MAX is used only for an initial memory-allocation sanity
   check, so it's OK to guess too small on hosts where there is no
   arbitrary limit to symbolic link length.  */
#ifndef SYMLINK_MAX
# define SYMLINK_MAX 1024
#endif

#define MAXSIZE (SIZE_MAX < SSIZE_MAX ? SIZE_MAX : SSIZE_MAX)

/* Call readlink to get the symbolic link value of FILE.
   SIZE is a hint as to how long the link is expected to be;
   typically it is taken from st_size.  It need not be correct.
   Return a pointer to that NUL-terminated string in malloc'd storage.
   If readlink fails, malloc fails, or if the link value is longer
   than SSIZE_MAX, return NULL (caller may use errno to diagnose).  */

char *
areadlink_with_size (char const *file, size_t size)
{
  /* Some buggy file systems report garbage in st_size.  Defend
     against them by ignoring outlandish st_size values in the initial
     memory allocation.  */
  size_t symlink_max = SYMLINK_MAX;
  size_t INITIAL_LIMIT_BOUND = 8 * 1024;
  size_t initial_limit = (symlink_max < INITIAL_LIMIT_BOUND
                          ? symlink_max + 1
                          : INITIAL_LIMIT_BOUND);

  enum { stackbuf_size = 128 };

  /* The initial buffer size for the link value.  */
  size_t buf_size = (size == 0 ? stackbuf_size
                     : size < initial_limit ? size + 1 : initial_limit);

  while (1)
    {
      ssize_t r;
      size_t link_length;
      char stackbuf[stackbuf_size];
      char *buf = stackbuf;
      char *buffer = NULL;

      if (! (size == 0 && buf_size == stackbuf_size))
        {
          buf = buffer = malloc (buf_size);
          if (!buffer)
            {
              errno = ENOMEM;
              return NULL;
            }
        }

      r = readlink (file, buf, buf_size);
      link_length = r;

      if (r < 0)
        {
          free (buffer);
          return NULL;
        }

      if (link_length < buf_size)
        {
          buf[link_length] = 0;
          if (!buffer)
            {
              buffer = malloc (link_length + 1);
              if (buffer)
                return memcpy (buffer, buf, link_length + 1);
            }
          else if (link_length + 1 < buf_size)
            {
              /* Shrink BUFFER before returning it.  */
              char *shrinked_buffer = realloc (buffer, link_length + 1);
              if (shrinked_buffer != NULL)
                buffer = shrinked_buffer;
            }
          return buffer;
        }

      free (buffer);
      if (buf_size <= MAXSIZE / 2)
        buf_size *= 2;
      else if (buf_size < MAXSIZE)
        buf_size = MAXSIZE;
      else
        {
          errno = ENOMEM;
          return NULL;
        }
    }
}
