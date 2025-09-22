/* Fetch live process registers from TID.
   Copyright (C) 2014 Red Hat, Inc.
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

#ifdef __linux__
#if defined __arm__
# include <sys/types.h>
# include <sys/user.h>
# include <sys/ptrace.h>
#endif

#ifdef __aarch64__
# include <linux/uio.h>
# include <sys/user.h>
# include <sys/ptrace.h>
/* Deal with old glibc defining user_pt_regs instead of user_regs_struct.  */
# ifndef HAVE_SYS_USER_REGS
#  define user_regs_struct user_pt_regs
# endif
#endif
#endif

#define BACKEND arm_
#include "libebl_CPU.h"

bool
arm_set_initial_registers_tid (pid_t tid __attribute__ ((unused)),
			  ebl_tid_registers_t *setfunc __attribute__ ((unused)),
			       void *arg __attribute__ ((unused)))
{
#if !defined(__linux__) || (!defined __arm__ && !defined __aarch64__)
  return false;
#else	/* __arm__ || __aarch64__ */
#if defined __arm__
  struct user_regs user_regs;
  if (ptrace (PTRACE_GETREGS, tid, NULL, &user_regs) != 0)
    return false;

  Dwarf_Word dwarf_regs[16];
  /* R0..R12 SP LR PC */
  for (int i = 0; i < 16; i++)
    dwarf_regs[i] = user_regs.uregs[i];

  return setfunc (0, 16, dwarf_regs, arg);
#elif defined __aarch64__
  /* Compat mode: arm compatible code running on aarch64 */
  int i;
  struct user_regs_struct gregs;
  struct iovec iovec;
  iovec.iov_base = &gregs;
  iovec.iov_len = sizeof (gregs);
  if (ptrace (PTRACE_GETREGSET, tid, NT_PRSTATUS, &iovec) != 0)
    return false;

  Dwarf_Word dwarf_regs[16];
  /* R0..R12 SP LR PC, encoded as 32 bit quantities */
  uint32_t *u32_ptr = (uint32_t *) &gregs.regs[0];
  for (i = 0; i < 16; i++)
    dwarf_regs[i] = u32_ptr[i];

  return setfunc (0, 16, dwarf_regs, arg);
#else
# error "source file error, it cannot happen"
#endif
#endif
}
