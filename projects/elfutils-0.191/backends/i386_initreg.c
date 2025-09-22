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

#if (defined __i386__ || defined __x86_64__) && defined(__linux__)
# include <sys/types.h>
# include <sys/user.h>
# include <sys/ptrace.h>
#endif

#define BACKEND i386_
#include "libebl_CPU.h"

bool
i386_set_initial_registers_tid (pid_t tid __attribute__ ((unused)),
			  ebl_tid_registers_t *setfunc __attribute__ ((unused)),
				void *arg __attribute__ ((unused)))
{
#if (!defined __i386__ && !defined __x86_64__) || !defined(__linux__)
  return false;
#else /* __i386__ || __x86_64__ */
  struct user_regs_struct user_regs;
  if (ptrace (PTRACE_GETREGS, tid, NULL, &user_regs) != 0)
    return false;
  Dwarf_Word dwarf_regs[9];
# if defined __i386__
  dwarf_regs[0] = user_regs.eax;
  dwarf_regs[1] = user_regs.ecx;
  dwarf_regs[2] = user_regs.edx;
  dwarf_regs[3] = user_regs.ebx;
  dwarf_regs[4] = user_regs.esp;
  dwarf_regs[5] = user_regs.ebp;
  dwarf_regs[6] = user_regs.esi;
  dwarf_regs[7] = user_regs.edi;
  dwarf_regs[8] = user_regs.eip;
# elif defined __x86_64__
  dwarf_regs[0] = user_regs.rax;
  dwarf_regs[1] = user_regs.rcx;
  dwarf_regs[2] = user_regs.rdx;
  dwarf_regs[3] = user_regs.rbx;
  dwarf_regs[4] = user_regs.rsp;
  dwarf_regs[5] = user_regs.rbp;
  dwarf_regs[6] = user_regs.rsi;
  dwarf_regs[7] = user_regs.rdi;
  dwarf_regs[8] = user_regs.rip;
# else /* (__i386__ || __x86_64__) && (!__i386__ && !__x86_64__) */
#  error "source file error, it cannot happen"
# endif /* (__i386__ || __x86_64__) && (!__i386__ && !__x86_64__) */
  return setfunc (0, 9, dwarf_regs, arg);
#endif /* __i386__ || __x86_64__ */
}
