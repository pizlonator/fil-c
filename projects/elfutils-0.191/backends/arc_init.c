/* Initialization of ARC specific backend library.
   Copyright (C) 2022 Synopsys Inc.
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

/* More details on an ARC elf can be found at:
   https://github.com/foss-for-synopsys-dwc-arc-processors/ \
	   arc-ABI-manual/blob/master/arcv3-elf.md   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define BACKEND		arc_
#define RELOC_PREFIX	R_ARC_
#include "libebl_CPU.h"

/* This defines the common reloc hooks based on arc_reloc.def.  */
#include "common-reloc.c"

Ebl *
arc_init (Elf *elf __attribute__ ((unused)),
	  GElf_Half machine __attribute__ ((unused)),
	  Ebl *eh)
{
  arc_init_reloc (eh);
  HOOK (eh, machine_flag_check);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, section_type_name);

  /* /bld/gcc-stage2/arc-snps-linux-gnu/libgcc/libgcc.map.in
     #define __LIBGCC_DWARF_FRAME_REGISTERS__.  */
  eh->frame_nregs = 146;

  return eh;
}
