/* Convert a struct servent object to a string.
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

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <support/support.h>
#include <support/xmemstream.h>

char *
support_format_servent (const struct servent *e)
{
  if (e == NULL)
    return xasprintf ("error: (errno %d, %m)\n", errno);

  struct xmemstream mem;
  xopen_memstream (&mem);

  fprintf (mem.out, "name: %s\n", e->s_name);
  for (char **ap = e->s_aliases; *ap != NULL; ++ap)
    fprintf (mem.out, "alias: %s\n", *ap);
  fprintf (mem.out, "port: %d\n", ntohs (e->s_port));
  fprintf (mem.out, "proto: %s\n", e->s_proto);

  xfclose_memstream (&mem);
  return mem.buffer;
}
