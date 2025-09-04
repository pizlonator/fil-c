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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "tst-audit27mod.h"

#define TEST_NAME  "tst-audit27"

#define AUDIT27_COOKIE 0

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
    ck = AUDIT27_COOKIE;
  *cookie = ck;
  printf ("objopen: %ld, %s [%ld]\n", lmid, l_name, ck);
  return ck == -1 ? 0 : LA_FLG_BINDFROM | LA_FLG_BINDTO;
}

ElfW(Addr)
la_aarch64_gnu_pltenter (ElfW(Sym) *sym, unsigned int ndx, uintptr_t *refcook,
			 uintptr_t *defcook, La_aarch64_regs *regs,
			 unsigned int *flags, const char *symname,
			 long int *framesizep)
{
  printf ("pltenter: symname=%s, st_value=%#lx, ndx=%u, flags=%u\n",
	  symname, (long int) sym->st_value, ndx, *flags);

  if (strcmp (symname, "tst_audit27_func_float") == 0)
    {
      assert (regs->lr_vreg[0].s == FUNC_FLOAT_ARG0);
      assert (regs->lr_vreg[1].s == FUNC_FLOAT_ARG1);
      assert (regs->lr_vreg[2].s == FUNC_FLOAT_ARG2);
      assert (regs->lr_vreg[3].s == FUNC_FLOAT_ARG3);
      assert (regs->lr_vreg[4].s == FUNC_FLOAT_ARG4);
      assert (regs->lr_vreg[5].s == FUNC_FLOAT_ARG5);
      assert (regs->lr_vreg[6].s == FUNC_FLOAT_ARG6);
      assert (regs->lr_vreg[7].s == FUNC_FLOAT_ARG7);
    }
  else if (strcmp (symname, "tst_audit27_func_double") == 0)
    {
      assert (regs->lr_vreg[0].d == FUNC_DOUBLE_ARG0);
      assert (regs->lr_vreg[1].d == FUNC_DOUBLE_ARG1);
      assert (regs->lr_vreg[2].d == FUNC_DOUBLE_ARG2);
      assert (regs->lr_vreg[3].d == FUNC_DOUBLE_ARG3);
      assert (regs->lr_vreg[4].d == FUNC_DOUBLE_ARG4);
      assert (regs->lr_vreg[5].d == FUNC_DOUBLE_ARG5);
      assert (regs->lr_vreg[6].d == FUNC_DOUBLE_ARG6);
      assert (regs->lr_vreg[7].d == FUNC_DOUBLE_ARG7);
    }
  else if (strcmp (symname, "tst_audit27_func_ldouble") == 0)
    {
      assert (regs->lr_vreg[0].q == FUNC_LDOUBLE_ARG0);
      assert (regs->lr_vreg[1].q == FUNC_LDOUBLE_ARG1);
      assert (regs->lr_vreg[2].q == FUNC_LDOUBLE_ARG2);
      assert (regs->lr_vreg[3].q == FUNC_LDOUBLE_ARG3);
      assert (regs->lr_vreg[4].q == FUNC_LDOUBLE_ARG4);
      assert (regs->lr_vreg[5].q == FUNC_LDOUBLE_ARG5);
      assert (regs->lr_vreg[6].q == FUNC_LDOUBLE_ARG6);
      assert (regs->lr_vreg[7].q == FUNC_LDOUBLE_ARG7);
    }
  else
    abort ();

  assert (regs->lr_vpcs == 0);

  /* Clobber the q registers on exit.  */
  uint8_t v = 0xff;
  asm volatile ("dup v0.8b, %w0" : : "r" (v) : "v0");
  asm volatile ("dup v1.8b, %w0" : : "r" (v) : "v1");
  asm volatile ("dup v2.8b, %w0" : : "r" (v) : "v2");
  asm volatile ("dup v3.8b, %w0" : : "r" (v) : "v3");
  asm volatile ("dup v4.8b, %w0" : : "r" (v) : "v4");
  asm volatile ("dup v5.8b, %w0" : : "r" (v) : "v5");
  asm volatile ("dup v6.8b, %w0" : : "r" (v) : "v6");
  asm volatile ("dup v7.8b, %w0" : : "r" (v) : "v7");

  *framesizep = 1024;

  return sym->st_value;
}

unsigned int
la_aarch64_gnu_pltexit (ElfW(Sym) *sym, unsigned int ndx, uintptr_t *refcook,
                        uintptr_t *defcook,
			const struct La_aarch64_regs *inregs,
                        struct La_aarch64_retval *outregs,
			const char *symname)
{
  printf ("pltexit: symname=%s, st_value=%#lx, ndx=%u\n",
	  symname, (long int) sym->st_value, ndx);

  if (strcmp (symname, "tst_audit27_func_float") == 0)
    {
      assert (inregs->lr_vreg[0].s == FUNC_FLOAT_ARG0);
      assert (inregs->lr_vreg[1].s == FUNC_FLOAT_ARG1);
      assert (inregs->lr_vreg[2].s == FUNC_FLOAT_ARG2);
      assert (inregs->lr_vreg[3].s == FUNC_FLOAT_ARG3);
      assert (inregs->lr_vreg[4].s == FUNC_FLOAT_ARG4);
      assert (inregs->lr_vreg[5].s == FUNC_FLOAT_ARG5);
      assert (inregs->lr_vreg[6].s == FUNC_FLOAT_ARG6);
      assert (inregs->lr_vreg[7].s == FUNC_FLOAT_ARG7);

      assert (outregs->lrv_vreg[0].s == FUNC_FLOAT_RET);
    }
  else if (strcmp (symname, "tst_audit27_func_double") == 0)
    {
      assert (inregs->lr_vreg[0].d == FUNC_DOUBLE_ARG0);
      assert (inregs->lr_vreg[1].d == FUNC_DOUBLE_ARG1);
      assert (inregs->lr_vreg[2].d == FUNC_DOUBLE_ARG2);
      assert (inregs->lr_vreg[3].d == FUNC_DOUBLE_ARG3);
      assert (inregs->lr_vreg[4].d == FUNC_DOUBLE_ARG4);
      assert (inregs->lr_vreg[5].d == FUNC_DOUBLE_ARG5);
      assert (inregs->lr_vreg[6].d == FUNC_DOUBLE_ARG6);
      assert (inregs->lr_vreg[7].d == FUNC_DOUBLE_ARG7);

      assert (outregs->lrv_vreg[0].d == FUNC_DOUBLE_RET);
    }
  else if (strcmp (symname, "tst_audit27_func_ldouble") == 0)
    {
      assert (inregs->lr_vreg[0].q == FUNC_LDOUBLE_ARG0);
      assert (inregs->lr_vreg[1].q == FUNC_LDOUBLE_ARG1);
      assert (inregs->lr_vreg[2].q == FUNC_LDOUBLE_ARG2);
      assert (inregs->lr_vreg[3].q == FUNC_LDOUBLE_ARG3);
      assert (inregs->lr_vreg[4].q == FUNC_LDOUBLE_ARG4);
      assert (inregs->lr_vreg[5].q == FUNC_LDOUBLE_ARG5);
      assert (inregs->lr_vreg[6].q == FUNC_LDOUBLE_ARG6);
      assert (inregs->lr_vreg[7].q == FUNC_LDOUBLE_ARG7);

      assert (outregs->lrv_vreg[0].q == FUNC_LDOUBLE_RET);
    }
  else
    abort ();

  assert (inregs->lr_vpcs == 0);
  assert (outregs->lrv_vpcs == 0);

  /* Clobber the q registers on exit.  */
  uint8_t v = 0xff;
  asm volatile ("dup v0.8b, %w0" : : "r" (v) : "v0");
  asm volatile ("dup v1.8b, %w0" : : "r" (v) : "v1");
  asm volatile ("dup v2.8b, %w0" : : "r" (v) : "v2");
  asm volatile ("dup v3.8b, %w0" : : "r" (v) : "v3");
  asm volatile ("dup v4.8b, %w0" : : "r" (v) : "v4");
  asm volatile ("dup v5.8b, %w0" : : "r" (v) : "v5");
  asm volatile ("dup v6.8b, %w0" : : "r" (v) : "v6");
  asm volatile ("dup v7.8b, %w0" : : "r" (v) : "v7");

  return 0;
}
