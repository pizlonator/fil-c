/* unistd.h related helpers.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#ifndef _UNISTD_EXT_H
#define _UNISTD_EXT_H

#include <error.h>
#include <errno.h>
#include <libintl.h>
#include <unistd.h>

/* Helpers used in catgets/gencat.c and malloc/memusage*.c */
static inline void
write_all (int fd, const void *buffer, size_t length)
{
  const char *p = buffer;
  const char *end = p + length;
  while (p < end)
    {
      ssize_t ret = write (fd, p, end - p);
      if (ret < 0)
	error (EXIT_FAILURE, errno,
	       gettext ("write of %zu bytes failed after %td: %m"),
	       length, p - (const char *) buffer);

      if (ret == 0)
	error (EXIT_FAILURE, 0,
	       gettext ("write returned 0 after writing %td bytes of %zu"),
	       p - (const char *) buffer, length);
      p += ret;
    }
}

static inline void
read_all (int fd, void *buffer, size_t length)
{
  char *p = buffer;
  char *end = p + length;
  while (p < end)
    {
      ssize_t ret = read (fd, p, end - p);
      if (ret < 0)
	error (EXIT_FAILURE, errno,
	       gettext ("read of %zu bytes failed after %td: %m"),
	       length, p - (char *) buffer);

      p += ret;
    }
}

#endif
