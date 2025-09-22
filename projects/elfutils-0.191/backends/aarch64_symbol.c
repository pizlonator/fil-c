/* AArch64 specific symbolic name handling.
   Copyright (C) 2013, 2015, 2017 Red Hat, Inc.
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

#include <system.h>

#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND		aarch64_
#include "libebl_CPU.h"


/* Check for the simple reloc types.  */
Elf_Type
aarch64_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
			   int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
    case R_AARCH64_ABS64:
      return ELF_T_XWORD;
    case R_AARCH64_ABS32:
      return ELF_T_WORD;
    case R_AARCH64_ABS16:
      return ELF_T_HALF;

    default:
      return ELF_T_NUM;
    }
}

/* If this is the _GLOBAL_OFFSET_TABLE_ symbol, then it should point in
   the .got even if there is a .got.plt section.
   https://sourceware.org/ml/libc-ports/2013-06/msg00057.html
   https://bugzilla.redhat.com/show_bug.cgi?id=1201778
 */
bool
aarch64_check_special_symbol (Elf *elf, const GElf_Sym *sym,
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

/* A data mapping symbol is a symbol with "$d" name or "$d.<any...>" name,
   STT_NOTYPE, STB_LOCAL and st_size of zero. The indicate the stat of a
   sequence of data items.  */
bool
aarch64_data_marker_symbol (const GElf_Sym *sym, const char *sname)
{
  return (sym != NULL && sname != NULL
	  && sym->st_size == 0 && GELF_ST_BIND (sym->st_info) == STB_LOCAL
	  && GELF_ST_TYPE (sym->st_info) == STT_NOTYPE
	  && (strcmp (sname, "$d") == 0 || startswith (sname, "$d.")));
}

const char *
aarch64_dynamic_tag_name (int64_t tag, char *buf __attribute__ ((unused)),
			  size_t len __attribute__ ((unused)))
{
  switch (tag)
    {
    case DT_AARCH64_BTI_PLT:
      return "AARCH64_BTI_PLT";
    case DT_AARCH64_PAC_PLT:
      return "AARCH64_PAC_PLT";
    case DT_AARCH64_VARIANT_PCS:
      return "AARCH64_VARIANT_PCS";
    default:
      break;
    }
  return NULL;
}

bool
aarch64_dynamic_tag_check (int64_t tag)
{
  return (tag == DT_AARCH64_BTI_PLT
	  || tag == DT_AARCH64_PAC_PLT
	  || tag == DT_AARCH64_VARIANT_PCS);
}
