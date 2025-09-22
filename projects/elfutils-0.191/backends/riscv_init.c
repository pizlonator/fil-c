/* Initialization of RISC-V specific backend library.
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

#define BACKEND		riscv_
#define RELOC_PREFIX	R_RISCV_
#include "libebl_CPU.h"

#include "libelfP.h"

/* This defines the common reloc hooks based on riscv_reloc.def.  */
#include "common-reloc.c"

extern __typeof (EBLHOOK (return_value_location))
  riscv_return_value_location_lp64d attribute_hidden;

extern __typeof (EBLHOOK (return_value_location))
    riscv_return_value_location_lp64f attribute_hidden;

extern __typeof (EBLHOOK (return_value_location))
    riscv_return_value_location_lp64 attribute_hidden;

extern __typeof (EBLHOOK (core_note)) riscv64_core_note attribute_hidden;

Ebl *
riscv_init (Elf *elf,
	    GElf_Half machine __attribute__ ((unused)),
	    Ebl *eh)
{
  /* We handle it.  */
  riscv_init_reloc (eh);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, register_info);
  HOOK (eh, abi_cfi);
  HOOK (eh, disasm);
  /* gcc/config/ #define DWARF_FRAME_REGISTERS.  */
  eh->frame_nregs = 66;
  HOOK (eh, check_special_symbol);
  HOOK (eh, machine_flag_check);
  HOOK (eh, set_initial_registers_tid);
  HOOK (eh, segment_type_name);
  HOOK (eh, section_type_name);
  HOOK (eh, dynamic_tag_name);
  HOOK (eh, dynamic_tag_check);
  if (eh->class == ELFCLASS64)
    eh->core_note = riscv64_core_note;
  else
    HOOK (eh, core_note);
  if (eh->class == ELFCLASS64)
    {
      if ((elf->state.elf64.ehdr->e_flags & EF_RISCV_FLOAT_ABI)
          == EF_RISCV_FLOAT_ABI_DOUBLE)
        eh->return_value_location = riscv_return_value_location_lp64d;
      else if ((elf->state.elf64.ehdr->e_flags & EF_RISCV_FLOAT_ABI)
               == EF_RISCV_FLOAT_ABI_SINGLE)
        eh->return_value_location = riscv_return_value_location_lp64f;
      else
        eh->return_value_location = riscv_return_value_location_lp64;
    }

  return eh;
}
