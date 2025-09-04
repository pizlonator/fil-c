/* Audit modules for tst-audit24b.
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
#include <tst-auditmod24.h>

#define TEST_NAME "tst-audit24b"
#define TEST_FUNC "tst_audit24b"

#define AUDIT24_COOKIE     0x1
#define AUDIT24MOD1_COOKIE 0x2
#define AUDIT24MOD2_COOKIE 0x3

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
  if (strcmp (l_name, TEST_NAME "mod1.so") == 0)
    ck = AUDIT24MOD1_COOKIE;
  else if (strcmp (l_name, TEST_NAME "mod2.so") == 0)
    ck = AUDIT24MOD2_COOKIE;
  else if (strcmp (l_name, TEST_NAME) == 0)
    ck = AUDIT24_COOKIE;

  *cookie = ck;
  return ck == -1 ? 0 : LA_FLG_BINDFROM | LA_FLG_BINDTO;
}

static int
tst_func1 (void)
{
  return 1;
}

static int
tst_func2 (void)
{
  return 2;
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
  if (*refcook == AUDIT24_COOKIE)
    {
      if (*defcook == AUDIT24MOD1_COOKIE)
	  {
	    if (strcmp (symname, TEST_FUNC "mod1_func1") == 0)
	      return (uintptr_t) tst_func1;
	    else if (strcmp (symname, TEST_FUNC "mod1_func2") == 0)
	      return sym->st_value;
	    abort ();
	  }
      /* malloc functions.  */
      return sym->st_value;
    }
  else if (*refcook == AUDIT24MOD1_COOKIE)
    {
      if (*defcook == AUDIT24MOD2_COOKIE
	  && (strcmp (symname, TEST_FUNC "mod2_func1") == 0))
	{
	  test_symbind_flags (*flags);
	  return (uintptr_t) tst_func2;
	}
    }

  abort ();
}
