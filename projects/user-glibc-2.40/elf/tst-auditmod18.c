/* Check DT_AUDIT with dlmopen.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
#include <link.h>

unsigned int
la_version (unsigned int version)
{
  fprintf (stderr, "%s\n", __func__);
  return LAV_CURRENT;
}

char *
la_objsearch (const char *name, uintptr_t *cookie, unsigned int flag)
{
  fprintf (stderr, "%s\n", __func__);
  return (char *) name;
}

void
la_activity (uintptr_t *cookie, unsigned int flag)
{
  fprintf (stderr, "%s\n", __func__);
}

unsigned int
la_objopen (struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
  fprintf (stderr, "%s\n", __func__);
  return LA_FLG_BINDTO | LA_FLG_BINDFROM;
}

unsigned int
la_objclose (uintptr_t *cookie)
{
  fprintf (stderr, "%s\n", __func__);
  return 0;
}

void
la_preinit (uintptr_t *cookie)
{
  fprintf (stderr, "%s\n", __func__);
}

uintptr_t
#if __ELF_NATIVE_CLASS == 32
la_symbind32 (Elf32_Sym *sym, unsigned int ndx, uintptr_t *refcook,
              uintptr_t *defcook, unsigned int *flags, const char *symname)
#else
la_symbind64 (Elf64_Sym *sym, unsigned int ndx, uintptr_t *refcook,
              uintptr_t *defcook, unsigned int *flags, const char *symname)
#endif
{
  fprintf (stderr, "%s\n", __func__);
  return sym->st_value;
}
