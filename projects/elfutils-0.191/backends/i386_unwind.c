/* Get previous frame state for an existing frame state using frame pointers.
   Copyright (C) 2017 Red Hat, Inc.
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
#include <assert.h>

#define BACKEND i386_
#include "libebl_CPU.h"

/* Register numbers for frame and stack pointers.  We take advantage of
   them being next to each other when calling getfunc and setfunc.  */
#define ESP 4
#define EBP (ESP + 1)

/* Most basic frame pointer chasing with EBP as frame pointer.
   PC = *(FP + 4), SP = FP + 8, FP = *FP.  */
bool
i386_unwind (Ebl *ebl __attribute__ ((unused)),
	     Dwarf_Addr pc __attribute__ ((unused)),
	     ebl_tid_registers_t *setfunc, ebl_tid_registers_get_t *getfunc,
	     ebl_pid_memory_read_t *readfunc, void *arg,
	     bool *signal_framep __attribute__ ((unused)))
{
  /* sp = 0, fp = 1 */
  Dwarf_Word regs[2];

  /* Get current stack and frame pointers.  */
  if (! getfunc (ESP, 2, regs, arg))
    return false;

  Dwarf_Word sp = regs[0];
  Dwarf_Word fp = regs[1];

  /* Sanity check.  We only support traditional stack frames.  */
  if (fp == 0 || sp == 0 || fp < sp)
    return false;

  /* Get the return address from the stack, it is our new pc.  */
  Dwarf_Word ret_addr;
  if (! readfunc (fp + 4, &ret_addr, arg) || ret_addr == 0)
    return false;

  /* Get new sp and fp.  Sanity check again.  */
  sp = fp + 8;
  if (! readfunc (fp, &fp, arg) || fp == 0 || sp >= fp)
    return false;

  /* Set new sp, fp and pc.  */
  regs[0] = sp;
  regs[1] = fp;
  if (! setfunc (ESP, 2, regs, arg) || ! setfunc (-1, 1, &ret_addr, arg))
    return false;

  return true;
}
