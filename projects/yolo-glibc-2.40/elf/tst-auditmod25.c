/* Audit modules for tst-audit25a.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define AUDIT25_COOKIE     0x1
#define AUDIT25MOD1_COOKIE 0x2
#define AUDIT25MOD2_COOKIE 0x3
#define AUDIT25MOD3_COOKIE 0x2
#define AUDIT25MOD4_COOKIE 0x3

#define TEST_NAME "tst-audit25"
#define TEST_MOD  "tst-audit25"
#define TEST_FUNC "tst_audit25"

unsigned int
la_version (unsigned int version)
{
  return LAV_CURRENT;
}

unsigned int
la_objopen (struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
  const char *p = strrchr (map->l_name, '/');
  const char *l_name = p == NULL ? TEST_NAME : p + 1;

  uintptr_t ck = -1;
  if (strcmp (l_name, TEST_MOD "mod1.so") == 0)
    ck = AUDIT25MOD1_COOKIE;
  else if (strcmp (l_name, TEST_MOD "mod2.so") == 0)
    ck = AUDIT25MOD2_COOKIE;
  else if (strcmp (l_name, TEST_MOD "mod3.so") == 0)
    ck = AUDIT25MOD3_COOKIE;
  else if (strcmp (l_name, TEST_MOD "mod4.so") == 0)
    ck = AUDIT25MOD4_COOKIE;
  else if (strncmp (l_name, TEST_NAME, strlen (TEST_NAME)) == 0)
    ck = AUDIT25_COOKIE;

  *cookie = ck;
  return ck == -1 ? 0 : LA_FLG_BINDFROM | LA_FLG_BINDTO;
}

#if __ELF_NATIVE_CLASS == 64
uintptr_t
la_symbind64 (Elf64_Sym *sym, unsigned int ndx,
	      uintptr_t *refcook, uintptr_t *defcook,
	      unsigned int *flags, const char *symname)
#else
uintptr_t
la_symbind32 (Elf32_Sym *sym, unsigned int ndx,
	      uintptr_t *refcook, uintptr_t *defcook,
	      unsigned int *flags, const char *symname)
#endif
{
  if (*refcook != -1 && *defcook != -1 && symname[0] != '\0'
      && (*flags & LA_SYMB_DLSYM) == 0)
    fprintf (stderr, "la_symbind: %s %u\n", symname,
	     *flags & (LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT) ? 1 : 0);
  return sym->st_value;
}
