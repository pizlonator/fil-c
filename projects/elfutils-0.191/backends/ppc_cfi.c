/* ppc ABI-specified defaults for DWARF CFI.
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

#define BACKEND ppc_
#include "libebl_CPU.h"

int
ppc_abi_cfi (Ebl *ebl __attribute__ ((unused)), Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      /* This instruction is provided in every CIE.  It is not repeated here:
	 DW_CFA_def_cfa, ULEB128_7 (1), ULEB128_7 (0)  */
      /* r1 is assumed to be restored from cfa address,
	 r1 acts as a stack frame pointer.  */
      DW_CFA_val_offset, ULEB128_7 (1), ULEB128_7 (0),
      /* lr is not callee-saved but it needs to be preserved as it is pre-set
	 by the caller.  */
      DW_CFA_same_value, ULEB128_7 (65), /* lr */

      /* Callee-saved regs.  */
#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      SV (2),			/* r2 is TOC pointer.  */
      SV (13),			/* Reserved as system thread id (is it for CFI?).  */
      /* r14-r31 are non-volatile registers.  */
      SV (14), SV (15), SV (16), SV (17), SV (18), SV (19), SV (20), SV (21),
      SV (22), SV (23), SV (24), SV (25), SV (26), SV (27), SV (28), SV (29),
      SV (30), SV (31)
      /* VMX registers v20-v31 and vrsave are non-volatile but they are
	 assigned DWARF registers 1144-1156 (v20-v31) which is outside of the
	 CFI supported range.  */
#undef SV
    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = ebl->class == ELFCLASS64 ? 8 : 4;

  abi_info->return_address_register = 65;

  return 0;
}

__typeof (ppc_abi_cfi)
     ppc64_abi_cfi
     __attribute__ ((alias ("ppc_abi_cfi")));
