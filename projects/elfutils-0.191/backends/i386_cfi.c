/* i386 ABI-specified defaults for DWARF CFI.
   Copyright (C) 2009 Red Hat, Inc.
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

#define BACKEND i386_
#include "libebl_CPU.h"

int
i386_abi_cfi (Ebl *ebl __attribute__ ((unused)), Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      /* Call-saved regs.  */
      DW_CFA_same_value, ULEB128_7 (3), /* %ebx */
      DW_CFA_same_value, ULEB128_7 (5), /* %ebp */
      DW_CFA_same_value, ULEB128_7 (6), /* %esi */
      DW_CFA_same_value, ULEB128_7 (7), /* %edi */

      /* The CFA is the SP.  */
      DW_CFA_val_offset, ULEB128_7 (4), ULEB128_7 (0),

      /* Segment registers are call-saved if ever used at all.  */
      DW_CFA_same_value, ULEB128_7 (40), /* %es */
      DW_CFA_same_value, ULEB128_7 (41), /* %cs */
      DW_CFA_same_value, ULEB128_7 (42), /* %ss */
      DW_CFA_same_value, ULEB128_7 (43), /* %ds */
      DW_CFA_same_value, ULEB128_7 (44), /* %fs */
      DW_CFA_same_value, ULEB128_7 (45), /* %gs */
    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = 4;

  abi_info->return_address_register = 8; /* %eip */

  return 0;
}
