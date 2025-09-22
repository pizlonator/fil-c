/* arm64 ABI-specified defaults for DWARF CFI.
   Copyright (C) 2013, 2018 Red Hat, Inc.
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

#include <dwarf.h>

#define BACKEND aarch64_
#include "libebl_CPU.h"


/* ABI-specified state of DWARF CFI based on:

   "DWARF for the ARM 64 bit architecture (AArch64) 1.0"
http://infocenter.arm.com/help/topic/com.arm.doc.ihi0057b/IHI0057B_aadwarf64.pdf

   "Procedure Call Standard for the ARM 64 bit Architecture 1.0"
http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
*/

int
aarch64_abi_cfi (Ebl *ebl __attribute__ ((unused)), Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      /* The initial Canonical Frame Address is the value of the
         Stack Pointer (r31) as setup in the previous frame. */
      DW_CFA_def_cfa, ULEB128_7 (30), ULEB128_7 (0),

#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      /* Callee-saved regs r19-r28.  */
      SV (19), SV (20), SV (21), SV (22), SV (23),
      SV (24), SV (25), SV (26), SV (27), SV (28),

      /* The Frame Pointer (FP, r29) and Link Register (LR, r30).  */
      SV (29), SV (30),

      /* The Stack Pointer (r31) is restored from CFA address by default.  */
      DW_CFA_val_offset, ULEB128_7 (31), ULEB128_7 (0),

      /* Callee-saved fpregs v8-v15.  v0 == 64.  */
      SV (72), SV (73), SV (74), SV (75),
      SV (76), SV (77), SV (78), SV (79),
#undef SV

      /* XXX Note: registers intentionally unused by the program,
	 for example as a consequence of the procedure call standard
	 should be initialized as if by DW_CFA_same_value.  */
    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = -4;

  abi_info->return_address_register = 30; /* lr.  */

  return 0;
}
