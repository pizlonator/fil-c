/* Check LD_AUDIT for aarch64 specific ABI.
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

#include <assert.h>
#include <link.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tst-audit26mod.h"

#define TEST_NAME  "tst-audit26"

#define AUDIT26_COOKIE 0

unsigned int
la_version (unsigned int v)
{
  return v;
}

unsigned int
la_objopen (struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
  const char *p = strrchr (map->l_name, '/');
  const char *l_name = p == NULL ? map->l_name : p + 1;
  uintptr_t ck = -1;
  if (strncmp (l_name, TEST_NAME, strlen (TEST_NAME)) == 0)
    ck = AUDIT26_COOKIE;
  *cookie = ck;
  printf ("objopen: %ld, %s [cookie=%ld]\n", lmid, l_name, ck);
  return ck == -1 ? 0 : LA_FLG_BINDFROM | LA_FLG_BINDTO;
}

ElfW(Addr)
la_aarch64_gnu_pltenter (ElfW(Sym) *sym __attribute__ ((unused)),
                         unsigned int ndx __attribute__ ((unused)),
                         uintptr_t *refcook, uintptr_t *defcook,
                         La_aarch64_regs *regs, unsigned int *flags,
                         const char *symname, long int *framesizep)
{
  printf ("pltenter: symname=%s, st_value=%#lx, ndx=%u, flags=%u\n",
	  symname, (long int) sym->st_value, ndx, *flags);

  if (strcmp (symname, "tst_audit26_func") == 0)
    {
      assert (regs->lr_xreg[0] == ARG1);
      assert (regs->lr_xreg[1] == ARG2);
      assert (regs->lr_xreg[2] == ARG3);
    }
  else
    abort ();

  assert (regs->lr_vpcs == 0);

  /* Clobber 'x8'.  */
  asm volatile ("mov x8, -1" : : : "x8");

  *framesizep = 1024;

  return sym->st_value;
}

unsigned int
la_aarch64_gnu_pltexit (ElfW(Sym) *sym, unsigned int ndx, uintptr_t *refcook,
                        uintptr_t *defcook,
                        const struct La_aarch64_regs *inregs,
                        struct La_aarch64_retval *outregs, const char *symname)
{
  printf ("pltexit: symname=%s, st_value=%#lx, ndx=%u\n",
	  symname, (long int) sym->st_value, ndx);

  if (strcmp (symname, "tst_audit26_func") == 0)
    {
      assert (inregs->lr_xreg[0] == ARG1);
      assert (inregs->lr_xreg[1] == ARG2);
      assert (inregs->lr_xreg[2] == ARG3);
    }
  else
    abort ();

  assert (inregs->lr_vpcs == 0);
  assert (outregs->lrv_vpcs == 0);

  /* Clobber 'x8'.  */
  asm volatile ("mov x8, -1" : : : "x8");

  return 0;
}
