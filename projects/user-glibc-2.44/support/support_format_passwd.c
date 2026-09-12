/* Convert a struct passwd object to a string.
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

#include <errno.h>
#include <pwd.h>
#include <support/support.h>
#include <support/xmemstream.h>

char *
support_format_passwd (const struct passwd *p)
{
  if (p == NULL)
    return xasprintf ("error: (errno %d, %m)\n", errno);

  struct xmemstream mem;
  xopen_memstream (&mem);

  fprintf (mem.out, "name: %s\n", p->pw_name);
  fprintf (mem.out, "passwd: %s\n", p->pw_passwd);
  fprintf (mem.out, "uid: %u\n", (unsigned int) p->pw_uid);
  fprintf (mem.out, "gid: %u\n", (unsigned int) p->pw_gid);
  fprintf (mem.out, "gecos: %s\n", p->pw_gecos);
  fprintf (mem.out, "dir: %s\n", p->pw_dir);
  fprintf (mem.out, "shell: %s\n", p->pw_shell);

  xfclose_memstream (&mem);
  return mem.buffer;
}
