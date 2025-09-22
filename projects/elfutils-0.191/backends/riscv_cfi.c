/* RISC-V ABI-specified defaults for DWARF CFI.
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

#define BACKEND riscv64_
#include "libebl_CPU.h"


int
riscv_abi_cfi (Ebl *ebl __attribute__ ((unused)), Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      /* The initial Canonical Frame Address is the value of the
         Stack Pointer (r2) as setup in the previous frame. */
      DW_CFA_def_cfa, ULEB128_7 (2), ULEB128_7 (0),

      /* The Stack Pointer (r2) is restored from CFA address by default.  */
      DW_CFA_val_offset, ULEB128_7 (2), ULEB128_7 (0),

#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      /* The return address register contains the return address setup by
	 caller.  */
      SV (1),

      /* Callee-saved registers s0-s11, fs0-fs11.  */
      SV(8), SV (9), SV (18), SV (19), SV (20), SV (21),
      SV (22), SV (23), SV (24), SV (25), SV (26), SV (27),

      SV (40), SV (41),  SV (50),  SV (51), SV (52),  SV (53),
      SV (54), SV (55),  SV (56),  SV (57), SV (58),  SV (59),
#undef SV

      /* XXX Note: registers intentionally unused by the program,
	 for example as a consequence of the procedure call standard
	 should be initialized as if by DW_CFA_same_value.  */
    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = -4;

  abi_info->return_address_register = 1; /* ra.  */

  return 0;
}
