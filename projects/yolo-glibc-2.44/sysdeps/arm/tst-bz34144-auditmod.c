/* Minimal audit module used by tst-bz34144-audit to force PLT calls
   to go through _dl_runtime_profile instead of _dl_runtime_resolve.
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
#include <stddef.h>
#include <stdint.h>

unsigned int
la_version (unsigned int v)
{
  return v;
}

unsigned int
la_objopen (struct link_map *l, Lmid_t lmid, uintptr_t *cookie)
{
  return LA_FLG_BINDFROM | LA_FLG_BINDTO;
}

uintptr_t
la_symbind32 (Elf32_Sym *sym, unsigned int ndx, uintptr_t *refcook,
	      uintptr_t *defcook, unsigned int *flags, const char *symname)
{
  return sym->st_value;
}

Elf32_Addr
la_arm_gnu_pltenter (Elf32_Sym *sym, unsigned int ndx, uintptr_t *refcook,
		     uintptr_t *defcook, La_arm_regs *regs,
		     unsigned int *flags, const char *symname,
		     long int *framesizep)
{
  return sym->st_value;
}
