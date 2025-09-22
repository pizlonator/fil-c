/* Fetch live process registers from TID.
   Copyright (C) 2015 Oracle, In
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
#include <stdlib.h>
#ifdef __sparc__
# include <asm/ptrace.h>
# include <sys/ptrace.h>
#endif

#define BACKEND sparc_
#include "libebl_CPU.h"

bool
EBLHOOK (set_initial_registers_tid) (pid_t tid __attribute__ ((unused)),
                                     ebl_tid_registers_t *setfunc __attribute__ ((unused)),
                                     void *arg __attribute__ ((unused)))
{
#if !defined(__sparc__) || !defined( __arch64__)
  return false;
#else /* __sparc__ */


  /* The pt_regs structure filled in by PTRACE_GETREGS provides the
     PC, the global registers and the output registers.  Note how the
     %g0 register is not explicitly provided in the structure (it's
     value is always 0) and the resulting weird packing in the u_regs
     array: the last element is not used.  */
  
  struct pt_regs regs;
  if (ptrace (PTRACE_GETREGS, tid, &regs, 0) == -1)
    return false;

  /* PC: no DWARF number  */
  if (!setfunc (-1, 1, (Dwarf_Word *) &regs.tpc, arg))
    return false;
  
  /* Global registers: DWARF 0 .. 7  */
  Dwarf_Word zero = 0;
  if (!setfunc (0, 1, &zero, arg))
    return false;
  if (!setfunc (1, 7, (Dwarf_Word *) &regs.u_regs[0], arg))
    return false;

  /* Output registers: DWARF  8 .. 15  */
  if (!setfunc (8, 8, (Dwarf_Word *) &regs.u_regs[7], arg))
    return false;

  /* Local and input registers must be read from the stack.  They are
     saved in the previous stack frame.  The stack pointer is %o6,
     read above.  */

  Dwarf_Word locals_outs[16];
  Dwarf_Word sp = regs.u_regs[13];

  if (sp & 1)
    {
      /* Registers are 64 bits, and we need to apply the 2047 stack
         bias in order to get the real stack pointer.  */

      sp += 2047;

      for (unsigned i = 0; i < 16; i++)
        {
          locals_outs[i] = ptrace (PTRACE_PEEKDATA, tid,
                                   (void *) (uintptr_t) (sp + (i * 8)),
                                   NULL);
          if (errno != 0)
            return false;
        }
    }
  else
    {
      /* Registers are 32 bits.  */

      for (unsigned i = 0; i < 8; i++)
        {
          Dwarf_Word tuple = ptrace (PTRACE_PEEKDATA, tid,
                                     (void *) (uintptr_t) (sp + (i * 8)),
                                     NULL);
          if (errno != 0)
            return false;

          locals_outs[2*i] = (tuple >> 32) & 0xffffffff;
          locals_outs[2*i+1] = tuple & 0xffffffff;
        }
    }

  
  /* Local registers:  DWARF 16 .. 23 */
  if (!setfunc (16, 8, &locals_outs[0], arg))
    return false;
  
  /* Input registers: DWARF 24 .. 31 */
  if (!setfunc (24, 8, &locals_outs[8], arg))
    return false;

  return true;
#endif
}
