/* Copyright (C) 1992-2026 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <libc-diag.h>

int
main (int argc, char *argv[])
{
  char *buf = NULL;
  size_t size = 0;
  ssize_t len;

  while ((len = getline (&buf, &size, stdin)) != -1)
    {
      /* clang do not handle %Z format.  */
      DIAG_PUSH_NEEDS_COMMENT_CLANG;
      DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wformat-invalid-specifier");
      DIAG_IGNORE_NEEDS_COMMENT_CLANG (13, "-Wformat-extra-args");
      printf ("bufsize %Zu; read %Zd: ", size, len);
      DIAG_POP_NEEDS_COMMENT_CLANG;
      if (fwrite (buf, len, 1, stdout) != 1)
	{
	  perror ("fwrite");
	  return 1;
	}
    }

  if (ferror (stdin))
    {
      perror ("getline");
      return 1;
    }

  return 0;
}
