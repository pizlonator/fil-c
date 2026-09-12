/* Convert a struct aliasent object to a string.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <support/format_nss.h>

#include <aliases.h>
#include <errno.h>
#include <support/support.h>
#include <support/xmemstream.h>

char *
support_format_aliasent (const struct aliasent *e)
{
  if (e == NULL)
    xasprintf ("errno: (errno %d, %m)\n", errno);

  struct xmemstream mem;
  xopen_memstream (&mem);

  fprintf (mem.out, "name: %s\n", e->alias_name);
  for (size_t i = 0; i < e->alias_members_len; ++i)
    fprintf (mem.out, "member: %s\n", e->alias_members[i]);
  fprintf (mem.out, "local: %d\n", e->alias_local);

  xfclose_memstream (&mem);
  return mem.buffer;
}
