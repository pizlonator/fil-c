/* Get previous frame state for an existing frame state.
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

#define BACKEND ppc64_

#define LR_REG 65 /* Not 108, see ppc_dwarf_to_regno.  */
#define SP_REG  1

#define LR_OFFSET 16

#include "libebl_CPU.h"

/* Simplistic fallback frame unwinder. SP points to the backchain (contains
   address of previous stack pointer). At SP offset 16 is the LR save area
   (contains the value of the previous LR).  */

bool
EBLHOOK(unwind) (Ebl *ebl __attribute__ ((unused)),
		 Dwarf_Addr pc __attribute__ ((unused)),
                 ebl_tid_registers_t *setfunc, ebl_tid_registers_get_t *getfunc,
                 ebl_pid_memory_read_t *readfunc, void *arg,
                 bool *signal_framep __attribute__ ((unused)))
{
  Dwarf_Word sp, newSp, lr, newLr;

  /* Stack pointer points to the backchain which contains the previous sp.  */
  if (! getfunc (SP_REG, 1, &sp, arg))
    sp = 0;

  /* Link register contains previous program counter.  */
  if (! getfunc (LR_REG, 1, &lr, arg)
      || lr == 0
      || ! setfunc (-1, 1, &lr, arg))
    return false;

  if (! readfunc(sp, &newSp, arg))
    newSp = 0;

  if (! readfunc(newSp + LR_OFFSET, &newLr, arg))
    newLr = 0;

  setfunc(SP_REG, 1, &newSp, arg);
  setfunc(LR_REG, 1, &newLr, arg);

  /* Sanity check the stack grows down.  */
  return newSp > sp;
}
