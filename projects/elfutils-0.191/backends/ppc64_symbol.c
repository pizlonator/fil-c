/* PPC64 specific symbolic name handling.
   Copyright (C) 2004, 2005, 2014 Red Hat, Inc.
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

#include <assert.h>
#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND		ppc64_
#include "libebl_CPU.h"


/* Check for the simple reloc types.  */
Elf_Type
ppc64_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
			 int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
    case R_PPC64_ADDR64:
    case R_PPC64_UADDR64:
      return ELF_T_XWORD;
    case R_PPC64_ADDR32:
    case R_PPC64_UADDR32:
      return ELF_T_WORD;
    case R_PPC64_UADDR16:
      return ELF_T_HALF;
    default:
      return ELF_T_NUM;
    }
}


const char *
ppc64_dynamic_tag_name (int64_t tag, char *buf __attribute__ ((unused)),
			size_t len __attribute__ ((unused)))
{
  switch (tag)
    {
    case DT_PPC64_GLINK:
      return "PPC64_GLINK";
    case DT_PPC64_OPD:
      return "PPC64_OPD";
    case DT_PPC64_OPDSZ:
      return "PPC64_OPDSZ";
    case DT_PPC64_OPT:
      return "PPC64_OPT";
    default:
      break;
    }
  return NULL;
}


bool
ppc64_dynamic_tag_check (int64_t tag)
{
  return (tag == DT_PPC64_GLINK
	  || tag == DT_PPC64_OPD
	  || tag == DT_PPC64_OPDSZ
	  || tag == DT_PPC64_OPT);
}


/* Check whether given symbol's st_value and st_size are OK despite failing
   normal checks.  */
bool
ppc64_check_special_symbol (Elf *elf,
			    const GElf_Sym *sym __attribute__ ((unused)),
			    const char *name __attribute__ ((unused)),
			    const GElf_Shdr *destshdr)
{
  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx) != 0)
    return false;
  const char *sname = elf_strptr (elf, shstrndx, destshdr->sh_name);
  if (sname == NULL)
    return false;
  return strcmp (sname, ".opd") == 0;
}


/* Check if backend uses a bss PLT in this file.  */
bool
ppc64_bss_plt_p (Elf *elf __attribute__ ((unused)))
{
  return true;
}

/* Check whether machine flags are valid.  PPC64 has three possible values:
   0 - for unspecified ABI, or not using any specific ABI features.
   1 - for the original ELF PPC64 ABI using function descriptors.
   2 - for the revised ELFv2 PPC64 ABI without function descriptors.  */
bool
ppc64_machine_flag_check (GElf_Word flags)
{
  return flags == 0 || flags == 1 || flags == 2;
}

bool
ppc64_check_st_other_bits (unsigned char st_other)
{
  return (PPC64_LOCAL_ENTRY_OFFSET (st_other) != 0);
}
