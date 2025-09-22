/* s390 ABI-specified defaults for DWARF CFI.
   Copyright (C) 2012, 2013 Red Hat, Inc.
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

#define BACKEND s390_
#include "libebl_CPU.h"

int
s390_abi_cfi (Ebl *ebl, Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      /* This instruction is provided in every CIE.  It is not repeated here:
	 DW_CFA_def_cfa, ULEB128_7 (15), ULEB128_7 (96)  */
      /* r14 is not callee-saved but it needs to be preserved as it is pre-set
	 by the caller.  */
      DW_CFA_same_value, ULEB128_7 (14), /* r14 */

      /* Callee-saved regs.  */
#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      SV (6), SV (7), SV (8), SV (9), SV (10),		       /* r6-r13, r15 */
      SV (11), SV (12), SV (13), SV (15),
      SV (16 + 8), SV (16 + 9), SV (16 + 10), SV (16 + 11),    /* f8-f15 */
      SV (16 + 12), SV (16 + 13), SV (16 + 14), SV (16 + 15)
#undef SV
    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = ebl->class == ELFCLASS64 ? 8 : 4;

  abi_info->return_address_register = 14;

  return 0;
}
