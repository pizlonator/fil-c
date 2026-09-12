/* Audit DSO for GCS testing.
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

#include <link.h>

#define TST_COOKIE 0x1

unsigned int
la_version (unsigned int current)
{
  return LAV_CURRENT;
}

unsigned int la_objopen (struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
  return LA_FLG_BINDFROM | LA_FLG_BINDTO;
}

static int fun_wrapper (void)
{
  return 42;
}

uintptr_t
la_symbind64 (Elf64_Sym *sym, unsigned int ndx,
	      uintptr_t *refcook, uintptr_t *defcook,
	      unsigned int *flags, const char *symname)
{
  return strcmp (symname, "fun") == 0
    ? (uintptr_t) fun_wrapper : sym->st_value;
}
