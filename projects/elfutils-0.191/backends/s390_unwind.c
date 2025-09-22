/* Get previous frame state for an existing frame state.
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

#include <stdlib.h>
#include <assert.h>

#define BACKEND s390_
#include "libebl_CPU.h"

/* s390/s390x do not annotate signal handler frame by CFI.  It would be also
   difficult as PC points into a stub built on stack.  Function below is called
   only if unwinder could not find CFI.  Function then verifies the register
   state for this frame really belongs to a signal frame.  In such case it
   fetches original registers saved by the signal frame.  */

bool
s390_unwind (Ebl *ebl, Dwarf_Addr pc, ebl_tid_registers_t *setfunc,
	     ebl_tid_registers_get_t *getfunc, ebl_pid_memory_read_t *readfunc,
	     void *arg, bool *signal_framep)
{
  /* Caller already assumed caller adjustment but S390 instructions are 4 bytes
     long.  Undo it.  */
  if ((pc & 0x3) != 0x3)
    return false;
  pc++;
  /* We can assume big-endian read here.  */
  Dwarf_Word instr;
  if (! readfunc (pc, &instr, arg))
    return false;
  /* Fetch only the very first two bytes.  */
  instr = (instr >> (ebl->class == ELFCLASS64 ? 48 : 16)) & 0xffff;
  /* See GDB s390_sigtramp_frame_sniffer.  */
  /* Check for 'svc' as the first instruction.  */
  if (((instr >> 8) & 0xff) != 0x0a)
    return false;
  /* Check for 'sigreturn' or 'rt_sigreturn' as the second instruction.  */
  if ((instr & 0xff) != 119 && (instr & 0xff) != 173)
    return false;
  /* See GDB s390_sigtramp_frame_unwind_cache.  */
  Dwarf_Word this_sp;
  if (! getfunc (0 + 15, 1, &this_sp, arg))
    return false;
  unsigned word_size = ebl->class == ELFCLASS64 ? 8 : 4;
  Dwarf_Addr next_cfa = this_sp + 16 * word_size + 32;
  /* "New-style RT frame" is not supported,
     assuming "Old-style RT frame and all non-RT frames".
     Pointer to the array of saved registers is at NEXT_CFA + 8.  */
  Dwarf_Word sigreg_ptr;
  if (! readfunc (next_cfa + 8, &sigreg_ptr, arg))
    return false;
  /* Skip PSW mask.  */
  sigreg_ptr += word_size;
  /* Read PSW address.  */
  Dwarf_Word val;
  if (! readfunc (sigreg_ptr, &val, arg))
    return false;
  if (! setfunc (-1, 1, &val, arg))
    return false;
  sigreg_ptr += word_size;
  /* Then the GPRs.  */
  Dwarf_Word gprs[16];
  for (int i = 0; i < 16; i++)
    {
      if (! readfunc (sigreg_ptr, &gprs[i], arg))
	return false;
      sigreg_ptr += word_size;
    }
  /* Then the ACRs.  Skip them, they are not used in CFI.  */
  for (int i = 0; i < 16; i++)
    sigreg_ptr += 4;
  /* The floating-point control word.  */
  sigreg_ptr += 8;
  /* And finally the FPRs.  */
  Dwarf_Word fprs[16];
  for (int i = 0; i < 16; i++)
    {
      if (! readfunc (sigreg_ptr, &val, arg))
	return false;
      if (ebl->class == ELFCLASS32)
	{
	  Dwarf_Addr val_low;
	  if (! readfunc (sigreg_ptr + 4, &val_low, arg))
	    return false;
	  val = (val << 32) | val_low;
	}
      fprs[i] = val;
      sigreg_ptr += 8;
    }
  /* If we have them, the GPR upper halves are appended at the end.  */
  if (ebl->class == ELFCLASS32)
    {
      /* Skip signal number.  */
      sigreg_ptr += 4;
      for (int i = 0; i < 16; i++)
	{
	  if (! readfunc (sigreg_ptr, &val, arg))
	    return false;
	  Dwarf_Word val_low = gprs[i];
	  val = (val << 32) | val_low;
	  gprs[i] = val;
	  sigreg_ptr += 4;
	}
    }
  if (! setfunc (0, 16, gprs, arg))
    return false;
  if (! setfunc (16, 16, fprs, arg))
    return false;
  *signal_framep = true;
  return true;
}
