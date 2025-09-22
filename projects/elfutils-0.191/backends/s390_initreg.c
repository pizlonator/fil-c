/* Fetch live process registers from TID.
   Copyright (C) 2013 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "system.h"
#include <assert.h>
#if defined(__s390__) && defined(__linux__)
# include <sys/user.h>
# include <sys/ptrace.h>
# include <asm/ptrace.h>
#endif

#define BACKEND s390_
#include "libebl_CPU.h"

bool
s390_set_initial_registers_tid (pid_t tid __attribute__ ((unused)),
			  ebl_tid_registers_t *setfunc __attribute__ ((unused)),
				void *arg __attribute__ ((unused)))
{
#if !defined(__s390__) || !defined(__linux__)
  return false;
#else /* __s390__ */
  struct user user_regs;
  ptrace_area parea;
  parea.process_addr = (uintptr_t) &user_regs;
  parea.kernel_addr = 0;
  parea.len = sizeof (user_regs);
  if (ptrace (PTRACE_PEEKUSR_AREA, tid, &parea, NULL) != 0)
    return false;
  /* If we run as s390x we get the 64-bit registers of tid.
     But -m31 executable seems to use only the 32-bit parts of its
     registers so we ignore the upper half.  */
  Dwarf_Word dwarf_regs[16];
  for (unsigned u = 0; u < 16; u++)
    dwarf_regs[u] = user_regs.regs.gprs[u];
  if (! setfunc (0, 16, dwarf_regs, arg))
    return false;
  /* Avoid conversion double -> integer.  */
  eu_static_assert (sizeof user_regs.regs.fp_regs.fprs[0]
		    == sizeof dwarf_regs[0]);
  for (unsigned u = 0; u < 16; u++)
    {
      // Store the double bits as is in the Dwarf_Word without conversion.
      union
	{
	  double d;
	  Dwarf_Word w;
	} fpr = { .d = user_regs.regs.fp_regs.fprs[u] };
      dwarf_regs[u] = fpr.w;
    }

  if (! setfunc (16, 16, dwarf_regs, arg))
    return false;
  dwarf_regs[0] = user_regs.regs.psw.addr;
  return setfunc (-1, 1, dwarf_regs, arg);
#endif /* __s390__ */
}

void
s390_normalize_pc (Ebl *ebl __attribute__ ((unused)), Dwarf_Addr *pc)
{
  assert (ebl->class == ELFCLASS32);

  /* Clear S390 bit 31.  */
  *pc &= (1U << 31) - 1;
}
