/* Initialization of PPC64 specific backend library.
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2004.

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

#include <string.h>

#define BACKEND		ppc64_
#define RELOC_PREFIX	R_PPC64_
#include "libebl_CPU.h"

/* This defines the common reloc hooks based on ppc64_reloc.def.  */
#include "common-reloc.c"


Ebl *
ppc64_init (Elf *elf __attribute__ ((unused)),
	    GElf_Half machine __attribute__ ((unused)),
	    Ebl *eh)
{
  /* We handle it.  */
  ppc64_init_reloc (eh);
  HOOK (eh, reloc_simple_type);
  HOOK (eh, dynamic_tag_name);
  HOOK (eh, dynamic_tag_check);
  HOOK (eh, machine_flag_check);
  HOOK (eh, copy_reloc_p);
  HOOK (eh, check_special_symbol);
  HOOK (eh, check_st_other_bits);
  HOOK (eh, bss_plt_p);
  HOOK (eh, return_value_location);
  HOOK (eh, register_info);
  HOOK (eh, core_note);
  HOOK (eh, auxv_info);
  HOOK (eh, check_object_attribute);
  HOOK (eh, abi_cfi);
  /* gcc/config/ #define DWARF_FRAME_REGISTERS.  */
  eh->frame_nregs = (114 - 1) + 32;
  HOOK (eh, set_initial_registers_tid);
  HOOK (eh, dwarf_to_regno);
  HOOK (eh, unwind);
  HOOK (eh, resolve_sym_value);

  /* Find the function descriptor .opd table for resolve_sym_value.  */
  if (elf != NULL)
    {
      GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (elf, &ehdr_mem);
      size_t shstrndx;
      if (ehdr != NULL && ehdr->e_type != ET_REL
	  && elf_getshdrstrndx (elf, &shstrndx) == 0)
	{
	  /* We could also try through DT_PPC64_OPD and DT_PPC64_OPDSZ. */
	  GElf_Shdr opd_shdr_mem, *opd_shdr;
	  Elf_Scn *scn = NULL;
	  while ((scn = elf_nextscn (elf, scn)) != NULL)
	    {
	      opd_shdr = gelf_getshdr (scn, &opd_shdr_mem);
	      if (opd_shdr != NULL
		  && (opd_shdr->sh_flags & SHF_ALLOC) != 0
		  && opd_shdr->sh_type == SHT_PROGBITS
		  && opd_shdr->sh_size > 0)
		{
		  const char *name = elf_strptr (elf, shstrndx,
						 opd_shdr->sh_name);
		  if (name != NULL && strcmp (name, ".opd") == 0)
		    {
		      eh->fd_addr = opd_shdr->sh_addr;
		      eh->fd_data = elf_getdata (scn, NULL);
		      break;
		    }
		}
	    }
	}
    }

  return eh;
}
