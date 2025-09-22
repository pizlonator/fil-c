/* Initialization of AArch64 specific backend library.
   Copyright (C) 2013, 2017 Red Hat, Inc.
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

#define BACKEND		aarch64_
#define RELOC_PREFIX	R_AARCH64_
#include "libebl_CPU.h"

/* This defines the common reloc hooks based on aarch64_reloc.def.  */
#include "common-reloc.c"


Ebl *
aarch64_init (Elf *elf __attribute__ ((unused)),
	      GElf_Half machine __attribute__ ((unused)),
	      Ebl *eh)
{
  /* We handle it.  */
  aarch64_init_reloc (eh);
  HOOK (eh, register_info);
  HOOK (eh, core_note);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, return_value_location);
  HOOK (eh, check_special_symbol);
  HOOK (eh, dynamic_tag_name);
  HOOK (eh, dynamic_tag_check);
  HOOK (eh, data_marker_symbol);
  HOOK (eh, abi_cfi);

  /* X0-X30 (31 regs) + SP + 1 Reserved + ELR, 30 Reserved regs (34-43)
     + V0-V31 (32 regs, least significant 64 bits only)
     + ALT_FRAME_RETURN_COLUMN (used when LR isn't used) = 97 DWARF regs. */
  eh->frame_nregs = 97;
  HOOK (eh, set_initial_registers_tid);
  HOOK (eh, unwind);

  return eh;
}
