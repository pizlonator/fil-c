/* Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *
getenv (const char *name)
{
  if (__environ == NULL || name[0] == '\0')
    return NULL;

  size_t len = strlen (name);
  for (char **ep = __environ; *ep != NULL; ++ep)
    {
      if (name[0] == (*ep)[0]
	  && strncmp (name, *ep, len) == 0 && (*ep)[len] == '=')
	return *ep + len + 1;
    }

  return NULL;
}
libc_hidden_def (getenv)
