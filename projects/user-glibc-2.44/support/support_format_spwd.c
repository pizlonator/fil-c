/* Convert a struct spwd object to a string.
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
#include <shadow.h>
#include <support/support.h>
#include <support/xmemstream.h>

char *
support_format_spwd (const struct spwd *s)
{
  if (s == NULL)
    return xasprintf ("error: (errno %d, %m)\n", errno);

  struct xmemstream mem;
  xopen_memstream (&mem);

  fprintf (mem.out, "sp_namp: %s\n", s->sp_namp);
  fprintf (mem.out, "sp_pwdp: %s\n", s->sp_pwdp);
  fprintf (mem.out, "sp_lstchg: %ld\n", s->sp_lstchg);
  fprintf (mem.out, "sp_min: %ld\n", s->sp_min);
  fprintf (mem.out, "sp_max: %ld\n", s->sp_max);
  fprintf (mem.out, "sp_warn: %ld\n", s->sp_warn);
  fprintf (mem.out, "sp_inact: %ld\n", s->sp_inact);
  fprintf (mem.out, "sp_expire: %ld\n", s->sp_expire);
  fprintf (mem.out, "sp_flag: %lu\n", s->sp_flag);

  xfclose_memstream (&mem);
  return mem.buffer;
}
