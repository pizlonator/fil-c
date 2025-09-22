/* Get previous frame state for an existing frame state.
   Copyright (C) 2023 OpenAnolis community LoongArch SIG.
   Copyright (C) 2023 Loongson Technology Corporation Limited.
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

#define BACKEND loongarch_
#define RA_REG 1
#define SP_REG 3
#define FP_REG 22

#define RA_OFFSET 8
#define FP_OFFSET 16

#include "libebl_CPU.h"

/* There was no CFI. Maybe we happen to have a frame pointer and can unwind from that?  */

bool
EBLHOOK(unwind) (Ebl *ebl __attribute__ ((unused)), Dwarf_Addr pc __attribute__ ((unused)),
                 ebl_tid_registers_t *setfunc, ebl_tid_registers_get_t *getfunc,
                 ebl_pid_memory_read_t *readfunc, void *arg,
                 bool *signal_framep __attribute__ ((unused)))
{
  Dwarf_Word fp, ra, sp;

  if (!getfunc(RA_REG, 1, &ra, arg))
    return false;

  if (ra == 0 || !setfunc(-1, 1, &ra, arg))
    return false;

  if (!getfunc(FP_REG, 1, &fp, arg))
    fp = 0;

  if (!getfunc(SP_REG, 1, &sp, arg))
    sp = 0;

  Dwarf_Word newRa, newFp, newSp;

  if (!readfunc(fp - RA_OFFSET, &newRa, arg))
    newRa = 0;

  if (!readfunc(fp - FP_OFFSET, &newFp, arg))
    newFp = 0;

  newSp = fp;

  // These are not fatal if they don't work. They will just prevent unwinding at the next frame.
  setfunc(RA_REG, 1, &newRa, arg);
  setfunc(FP_REG, 1, &newFp, arg);
  setfunc(SP_REG, 1, &newSp, arg);

  // If the fp is invalid, we might still have a valid ra.
  // But if the fp is valid, then the stack should be moving in the right direction.
  return fp == 0 || newSp > sp;
}
