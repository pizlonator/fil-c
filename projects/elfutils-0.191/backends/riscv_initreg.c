/* Fetch live process registers from TID.
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
#if defined __riscv && defined __linux__
# include <sys/uio.h>
# include <sys/procfs.h>
# include <sys/ptrace.h>
#endif

#define BACKEND riscv_
#include "libebl_CPU.h"

bool
riscv_set_initial_registers_tid (pid_t tid __attribute__ ((unused)),
				 ebl_tid_registers_t *setfunc __attribute__ ((unused)),
				 void *arg __attribute__ ((unused)))
{
#if !defined __riscv || !defined __linux__
  return false;
#else /* __riscv */

  /* General registers.  */
  elf_gregset_t gregs;
  struct iovec iovec;
  iovec.iov_base = &gregs;
  iovec.iov_len = sizeof (gregs);
  if (ptrace (PTRACE_GETREGSET, tid, NT_PRSTATUS, &iovec) != 0)
    return false;

  /* X0 is constant 0.  */
  Dwarf_Word zero = 0;
  if (! setfunc (0, 1, &zero, arg))
    return false;

  /* X1..X31.  */
  if (! setfunc (1, 32, (Dwarf_Word *) &gregs[1], arg))
    return false;

  /* PC.  */
  if (! setfunc (-1, 1, (Dwarf_Word *) &gregs[0], arg))
    return false;

  /* FP registers not yet supported.  */

  return true;
#endif /* __riscv */
}
