/* Initialization of M68K specific backend library.
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

#define BACKEND		m68k_
#define RELOC_PREFIX	R_68K_
#include "libebl_CPU.h"

/* This defines the common reloc hooks based on m68k_reloc.def.  */
#include "common-reloc.c"


Ebl *
m68k_init (Elf *elf __attribute__ ((unused)),
	   GElf_Half machine __attribute__ ((unused)),
	   Ebl *eh)
{
  /* We handle it.  */
  m68k_init_reloc (eh);
  HOOK (eh, gotpc_reloc_check);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, return_value_location);
  HOOK (eh, register_info);
  HOOK (eh, core_note);
  HOOK (eh, abi_cfi);
  /* gcc/config/ #define DWARF_FRAME_REGISTERS.  */
  eh->frame_nregs = 25;
  HOOK (eh, set_initial_registers_tid);

  return eh;
}
