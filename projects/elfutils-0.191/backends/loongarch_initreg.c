/* Fetch live process registers from TID.
   Copyright (C) 2023 OpenAnolis community LoongArch SIG.
   Copyright (C) 2023 Loongson Technology Corporation Limted.
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
#if defined __loongarch__ && defined __linux__
# include <sys/uio.h>
# include <sys/procfs.h>
# include <sys/ptrace.h>
#endif

#define BACKEND loongarch_
#include "libebl_CPU.h"

bool
loongarch_set_initial_registers_tid (pid_t tid __attribute__ ((unused)),
				 ebl_tid_registers_t *setfunc __attribute__ ((unused)),
				 void *arg __attribute__ ((unused)))
{
#if !defined __loongarch__ || !defined __linux__
  return false;
#else /* __loongarch__ */

  /* General registers.  */
  struct user_regs_struct gregs;
  struct iovec iovec;
  iovec.iov_base = &gregs;
  iovec.iov_len = sizeof (gregs);
  if (ptrace (PTRACE_GETREGSET, tid, NT_PRSTATUS, &iovec) != 0)
    return false;

  /* $r0 is constant 0.  */
  Dwarf_Word zero = 0;
  if (! setfunc (0, 1, &zero, arg))
    return false;

  /* $r1-$r31.  */
  if (! setfunc (1, 32, (Dwarf_Word *) &gregs.regs[1], arg))
    return false;

  /* PC.  */
  if (! setfunc (-1, 1, (Dwarf_Word *) &gregs.csr_era, arg))
    return false;

  /* Floating-point registers (only 64bits are used).  */
  struct user_fp_struct fregs;
  iovec.iov_base = &fregs;
  iovec.iov_len = sizeof (fregs);
  if (ptrace (PTRACE_GETREGSET, tid, NT_FPREGSET, &iovec) != 0)
    return false;

  /* $f0-$f31 */
  if (! setfunc (32, 32, (Dwarf_Word *) &fregs.fpr[0], arg))
    return false;

  return true;
#endif /* __loongarch__ */
}
