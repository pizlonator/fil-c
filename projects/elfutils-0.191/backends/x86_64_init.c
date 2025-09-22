/* Initialization of x86-64 specific backend library.
   Copyright (C) 2002-2009, 2013, 2018 Red Hat, Inc.
   Copyright (C) H.J. Lu <hjl.tools@gmail.com>, 2015.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#define BACKEND		x86_64_
#define RELOC_PREFIX	R_X86_64_
#include "libebl_CPU.h"

/* This defines the common reloc hooks based on x86_64_reloc.def.  */
#include "common-reloc.c"

extern __typeof (EBLHOOK (core_note)) x32_core_note attribute_hidden;

Ebl *
x86_64_init (Elf *elf __attribute__ ((unused)),
	     GElf_Half machine __attribute__ ((unused)),
	     Ebl *eh)
{
  /* We handle it.  */
  x86_64_init_reloc (eh);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, section_type_name);
  if (eh->class == ELFCLASS32)
    eh->core_note = x32_core_note;
  else
    HOOK (eh, core_note);
  HOOK (eh, return_value_location);
  HOOK (eh, register_info);
  HOOK (eh, auxv_info);
  HOOK (eh, disasm);
  HOOK (eh, abi_cfi);
  /* gcc/config/ #define DWARF_FRAME_REGISTERS.  */
  eh->frame_nregs = 17;
  HOOK (eh, set_initial_registers_tid);
  HOOK (eh, unwind);
  HOOK (eh, check_reloc_target_type);

  return eh;
}
