/* Initialization of Alpha specific backend library.
   Copyright (C) 2002-2011 Red Hat, Inc.
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

#define BACKEND		alpha_
#define RELOC_PREFIX	R_ALPHA_
#include "libebl_CPU.h"

/* This defines the common reloc hooks based on alpha_reloc.def.  */
#include "common-reloc.c"


Ebl *
alpha_init (Elf *elf __attribute__ ((unused)),
	    GElf_Half machine __attribute__ ((unused)),
	    Ebl *eh)
{
  /* We handle it.  */
  alpha_init_reloc (eh);
  HOOK (eh, dynamic_tag_name);
  HOOK (eh, dynamic_tag_check);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, return_value_location);
  HOOK (eh, machine_section_flag_check);
  HOOK (eh, check_special_section);
  HOOK (eh, check_special_symbol);
  HOOK (eh, check_st_other_bits);
  HOOK (eh, register_info);
  HOOK (eh, core_note);
  HOOK (eh, auxv_info);
  eh->sysvhash_entrysize = sizeof (Elf64_Xword);

  return eh;
}
