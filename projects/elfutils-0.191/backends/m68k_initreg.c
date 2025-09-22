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

#if defined __m68k__ && defined __linux__
# include <sys/types.h>
# include <sys/user.h>
# include <sys/ptrace.h>
#endif

#define BACKEND m68k_
#include "libebl_CPU.h"

bool
m68k_set_initial_registers_tid (pid_t tid __attribute__ ((unused)),
				ebl_tid_registers_t *setfunc __attribute__ ((unused)),
				void *arg __attribute__ ((unused)))
{
#if !defined __m68k__ || !defined __linux__
  return false;
#else /* __m68k__ */
  struct user_regs_struct user_regs;
  if (ptrace (PTRACE_GETREGS, tid, NULL, &user_regs) != 0)
    return false;

  Dwarf_Word dwarf_regs[16];
  dwarf_regs[0] = user_regs.d0;
  dwarf_regs[1] = user_regs.d1;
  dwarf_regs[2] = user_regs.d2;
  dwarf_regs[3] = user_regs.d3;
  dwarf_regs[4] = user_regs.d4;
  dwarf_regs[5] = user_regs.d5;
  dwarf_regs[6] = user_regs.d6;
  dwarf_regs[7] = user_regs.d7;
  dwarf_regs[8] = user_regs.a0;
  dwarf_regs[9] = user_regs.a1;
  dwarf_regs[10] = user_regs.a2;
  dwarf_regs[11] = user_regs.a3;
  dwarf_regs[12] = user_regs.a4;
  dwarf_regs[13] = user_regs.a5;
  dwarf_regs[14] = user_regs.a6;
  dwarf_regs[15] = user_regs.usp;

  /* D0..D7, A0..A7.  */
  if (! setfunc (0, 16, dwarf_regs, arg))
    return false;

  /* PC.  */
  dwarf_regs[0] = user_regs.pc;
  if (! setfunc (24, 1, dwarf_regs, arg))
    return false;

  return true;
#endif /* __m68k__ */
}
