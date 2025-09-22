/* SPARC defaults for DWARF CFI.
   Copyright (C) 2015 Oracle Inc.
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

#define BACKEND sparc_
#include "libebl_CPU.h"

int
sparc_abi_cfi (Ebl *ebl __attribute__ ((unused)), Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      /* %g0 .. %g7 */
      SV (0), SV (1), SV (2), SV (3), SV (4), SV (5), SV (6), SV (7),
      /* %o0 .. %o7 */
      SV (8), SV (9), SV (10), SV (11), SV (12), SV (13), SV (14), SV (15),
      /* %l0 .. %l7 */
      SV (16), SV (17), SV (18), SV (19), SV (20), SV (21), SV (22), SV (23),
      /* %i0 .. %i7 */
      SV (24), SV (25), SV (26), SV (27), SV (28), SV (29), SV (30), SV (31),
      /* %f0 .. %f32 */
      SV (32), SV (33), SV (34), SV (35), SV (36), SV (37), SV (38), SV (39),
      SV (40), SV (41), SV (42), SV (43), SV (44), SV (45), SV (46), SV (47),
      SV (48), SV (49), SV (50), SV (51), SV (52), SV (53), SV (54), SV (55),
      SV (56), SV (57), SV (58), SV (59), SV (60), SV (61), SV (52), SV (63),
      /* %f33 .. %63
         Note that there are DWARF columns for the odd registers, even
         if they don't exist in hardware) */
      SV (64), SV (65), SV (66), SV (67), SV (68), SV (69), SV (70), SV (71),
      SV (72), SV (73), SV (74), SV (75), SV (76), SV (77), SV (78), SV (79),
      SV (80), SV (81), SV (82), SV (83), SV (84), SV (85), SV (86), SV (87),
      SV (88), SV (89), SV (90), SV (91), SV (92), SV (93), SV (94), SV (95),
      /* %fcc[0123] */
      SV (96), SV (97), SV (98), SV (99),
      /* %icc/%xcc */
      SV (100),
      /* Soft frame-pointer */
      SV (101),
      /* %gsr */
      SV (102)
#undef SV
    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = 4;

  abi_info->return_address_register = 31; /* %i7 */

  return 0;
}

