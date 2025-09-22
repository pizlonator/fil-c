/* S/390-specific symbolic name handling.
   Copyright (C) 2005 Red Hat, Inc.
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

#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND		s390_
#include "libebl_CPU.h"

/* Check for the simple reloc types.  */
Elf_Type
s390_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
			int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
    case R_390_64:
      return ELF_T_SXWORD;
    case R_390_32:
      return ELF_T_SWORD;
    case R_390_16:
      return ELF_T_HALF;
    case R_390_8:
      return ELF_T_BYTE;
    default:
      return ELF_T_NUM;
    }
}

/* The _GLOBAL_OFFSET_TABLE_ symbol might point to the DT_PLTGOT,
   which is in the .got section, even if the symbol itself is
   associated with the is a .got.plt section.
   https://sourceware.org/ml/binutils/2018-07/msg00200.html  */
bool
s390_check_special_symbol (Elf *elf, const GElf_Sym *sym,
                              const char *name, const GElf_Shdr *destshdr)
{
  if (name != NULL
      && strcmp (name, "_GLOBAL_OFFSET_TABLE_") == 0)
    {
      size_t shstrndx;
      if (elf_getshdrstrndx (elf, &shstrndx) != 0)
	return false;
      const char *sname = elf_strptr (elf, shstrndx, destshdr->sh_name);
      if (sname != NULL
	  && (strcmp (sname, ".got") == 0 || strcmp (sname, ".got.plt") == 0))
	{
	  Elf_Scn *scn = NULL;
	  while ((scn = elf_nextscn (elf, scn)) != NULL)
	    {
	      GElf_Shdr shdr_mem;
	      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	      if (shdr != NULL)
		{
		  sname = elf_strptr (elf, shstrndx, shdr->sh_name);
		  if (sname != NULL && strcmp (sname, ".got") == 0)
		    return (sym->st_value >= shdr->sh_addr
			    && sym->st_value < shdr->sh_addr + shdr->sh_size);
		}
	    }
	}
    }

  return false;
}
