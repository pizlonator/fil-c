/* Fetch live process registers from TID. C-SKY version.
   Copyright (C) 2019 Hangzhou C-SKY Microsystems co.,ltd.
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
#if defined __CSKY__ && defined __linux__
# include <sys/uio.h>
# include <sys/procfs.h>
# include <sys/ptrace.h>
#endif

#define BACKEND csky_
#include "libebl_CPU.h"

bool
csky_set_initial_registers_tid (pid_t tid __attribute__ ((unused)),
			ebl_tid_registers_t *setfunc __attribute__ ((unused)),
				void *arg __attribute__ ((unused)))
{
#if !defined __CSKY__ || !defined __linux__
  return false;
#else /* __CSKY__ */
  struct pt_regs user_regs;
  struct iovec iovec;
  iovec.iov_base = &user_regs;
  iovec.iov_len = sizeof (user_regs);
  if (ptrace (PTRACE_GETREGSET, tid, NT_PRSTATUS, &iovec) != 0)
    return false;

  Dwarf_Word dwarf_regs[38];

  /* lr.  */
  dwarf_regs[15] = user_regs.lr;
  /* sp.  */
  dwarf_regs[14] = user_regs.usp;
  /* r0 ~ r13.  */
  dwarf_regs[0] = user_regs.a0;
  dwarf_regs[1] = user_regs.a1;
  dwarf_regs[2] = user_regs.a2;
  dwarf_regs[3] = user_regs.a3;
  for (int i = 4; i < 14; i++)
    dwarf_regs[i] = user_regs.regs[i - 4];
  /* r ~ r13.  */
  for (int i = 16; i < 31; i++)
    dwarf_regs[i] = user_regs.exregs[i - 16];
  /* tls.  */
  dwarf_regs[31] = user_regs.tls;
  /* hi.  */
  dwarf_regs[36] = user_regs.rhi;
  /* lo.  */
  dwarf_regs[37] = user_regs.rlo;
  /* pc.  */
  dwarf_regs[32] = user_regs.pc;
  setfunc (-1, 1, &dwarf_regs[32], arg);

  return setfunc (0, 38, dwarf_regs, arg);
#endif
}
