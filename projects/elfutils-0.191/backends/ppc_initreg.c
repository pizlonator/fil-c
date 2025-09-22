/* Fetch live process registers from TID.
   Copyright (C) 2013 Red Hat, Inc.
   Copyright (C) 2022 Mark J. Wielaard <mark@klomp.org>
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

#include <stdlib.h>
#if defined(__powerpc__) && defined(__linux__)
# include <sys/ptrace.h>
# include <asm/ptrace.h>
# ifndef PTRACE_GETREGSET
#  include <linux/ptrace.h>
# endif
# include <sys/user.h>
# include <sys/uio.h>
#endif

#include "system.h"

#define BACKEND ppc_
#include "libebl_CPU.h"

bool
ppc_dwarf_to_regno (Ebl *ebl __attribute__ ((unused)), unsigned *regno)
{
  switch (*regno)
  {
    case 108:
      // LR uses both 65 and 108 numbers, there is no consistency for it.
      *regno = 65;
      return true;
    case 0 ... 107:
    case 109 ... (114 - 1) -1:
      return true;
    case 1200 ... 1231:
      *regno = *regno - 1200 + (114 - 1);
      return true;
    default:
      return false;
  }
  abort ();
}

__typeof (ppc_dwarf_to_regno)
     ppc64_dwarf_to_regno
     __attribute__ ((alias ("ppc_dwarf_to_regno")));

bool
ppc_set_initial_registers_tid (pid_t tid __attribute__ ((unused)),
			  ebl_tid_registers_t *setfunc __attribute__ ((unused)),
			       void *arg __attribute__ ((unused)))
{
#if !defined(__powerpc__) || !defined(__linux__)
  return false;
#else /* __powerpc__ */

/* pt_regs for 32bit processes. Same as 64bit pt_regs but all registers
   are 32bit instead of 64bit long.  */
#define GPRS 32
  struct pt_regs32
  {
    uint32_t gpr[GPRS];
    uint32_t nip;
    uint32_t msr;
    uint32_t orig_gpr3;
    uint32_t ctr;
    uint32_t link;
    uint32_t xer;
    uint32_t ccr;
    uint32_t mq;
    uint32_t trap;
    uint32_t dar;
    uint32_t dsisr;
    uint32_t result;
  };

  struct pt_regs regs;
  struct pt_regs32 *regs32 = (struct pt_regs32 *) &regs;
  struct iovec iovec;
  iovec.iov_base = &regs;
  iovec.iov_len = sizeof (regs);
  if (ptrace (PTRACE_GETREGSET, tid, NT_PRSTATUS, &iovec) != 0)
    return false;

  /* Did we get the full pt_regs or less (the 32bit pt_regs)?  */
  bool get32 = iovec.iov_len < sizeof (struct pt_regs);
  Dwarf_Word dwarf_regs[GPRS];
  for (unsigned gpr = 0; gpr < GPRS; gpr++)
    dwarf_regs[gpr] = get32 ? regs32->gpr[gpr] : regs.gpr[gpr];
  if (! setfunc (0, GPRS, dwarf_regs, arg))
    return false;
  // LR uses both 65 and 108 numbers, there is no consistency for it.
  Dwarf_Word link = get32 ? regs32->link : regs.link;
  if (! setfunc (65, 1, &link, arg))
    return false;
  /* Registers like msr, ctr, xer, dar, dsisr etc. are probably irrelevant
     for CFI.  */
  Dwarf_Word pc = get32 ? (Dwarf_Word) regs32->nip : regs.nip;
  return setfunc (-1, 1, &pc, arg);
#endif /* __powerpc__ */
}

__typeof (ppc_set_initial_registers_tid)
     ppc64_set_initial_registers_tid
     __attribute__ ((alias ("ppc_set_initial_registers_tid")));
