/* Initialization of SPARC specific backend library.
   Copyright (C) 2002, 2005, 2006, 2007, 2008 Red Hat, Inc.
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

#define BACKEND		sparc_
#define RELOC_PREFIX	R_SPARC_
#include "libebl_CPU.h"

/* In SPARC some relocations use the most significative 24 bits of the
   r_type field to encode a secondary addend.  Make sure the routines
   in common-reloc.c acknowledge this.  */
#define RELOC_TYPE_ID(type) ((type) & 0xff)

/* This defines the common reloc hooks based on sparc_reloc.def.  */
#include "common-reloc.c"

extern __typeof (EBLHOOK (core_note)) sparc64_core_note attribute_hidden;

Ebl *
sparc_init (Elf *elf __attribute__ ((unused)),
	    GElf_Half machine __attribute__ ((unused)),
	    Ebl *eh)
{
  /* We handle it.  */
  sparc_init_reloc (eh);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, machine_flag_check);
  HOOK (eh, check_special_section);
  HOOK (eh, symbol_type_name);
  HOOK (eh, dynamic_tag_name);
  HOOK (eh, dynamic_tag_check);
  if (eh->class == ELFCLASS64)
    eh->core_note = sparc64_core_note;
  else
    HOOK (eh, core_note);
  HOOK (eh, auxv_info);
  HOOK (eh, register_info);
  HOOK (eh, return_value_location);
  HOOK (eh, check_object_attribute);
  HOOK (eh, abi_cfi);
  /* gcc/config/sparc.h define FIRST_PSEUDO_REGISTER  */
  eh->frame_nregs = 103;
  /* The CFI Dwarf register with the "return address" in sparc
     actually contains the call address.  The return address is
     located 8 bytes after it.  */
  eh->ra_offset = 8;
  HOOK (eh, set_initial_registers_tid);

  return eh;
}
