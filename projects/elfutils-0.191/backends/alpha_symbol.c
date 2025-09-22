/* Alpha specific symbolic name handling.
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

#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND		alpha_
#include "libebl_CPU.h"


const char *
alpha_dynamic_tag_name (int64_t tag, char *buf __attribute__ ((unused)),
			size_t len __attribute__ ((unused)))
{
  switch (tag)
    {
    case DT_ALPHA_PLTRO:
      return "ALPHA_PLTRO";
    default:
      break;
    }
  return NULL;
}

bool
alpha_dynamic_tag_check (int64_t tag)
{
  return tag == DT_ALPHA_PLTRO;
}

/* Check for the simple reloc types.  */
Elf_Type
alpha_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
			 int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
    case R_ALPHA_REFLONG:
      return ELF_T_WORD;
    case R_ALPHA_REFQUAD:
      return ELF_T_XWORD;
    default:
      return ELF_T_NUM;
    }
}


/* Check whether SHF_MASKPROC flags are valid.  */
bool
alpha_machine_section_flag_check (GElf_Xword sh_flags)
{
  return (sh_flags &~ (SHF_ALPHA_GPREL)) == 0;
}

bool
alpha_check_special_section (Ebl *ebl,
			     int ndx __attribute__ ((unused)),
			     const GElf_Shdr *shdr,
			     const char *sname __attribute__ ((unused)))
{
  if ((shdr->sh_flags
       & (SHF_WRITE | SHF_EXECINSTR)) == (SHF_WRITE | SHF_EXECINSTR)
      && shdr->sh_addr != 0)
    {
      /* This is ordinarily flagged, but is valid for an old-style PLT.

	 Look for the SHT_DYNAMIC section and the DT_PLTGOT tag in it.
	 Its d_ptr should match the .plt section's sh_addr.  */

      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (ebl->elf, scn)) != NULL)
	{
	  GElf_Shdr scn_shdr;
	  if (likely (gelf_getshdr (scn, &scn_shdr) != NULL)
	      && scn_shdr.sh_type == SHT_DYNAMIC
	      && scn_shdr.sh_entsize != 0)
	    {
	      GElf_Addr pltgot = 0;
	      Elf_Data *data = elf_getdata (scn, NULL);
	      if (data != NULL)
		for (size_t i = 0; i < data->d_size / scn_shdr.sh_entsize; ++i)
		  {
		    GElf_Dyn dyn;
		    if (unlikely (gelf_getdyn (data, i, &dyn) == NULL))
		      break;
		    if (dyn.d_tag == DT_PLTGOT)
		      pltgot = dyn.d_un.d_ptr;
		    else if (dyn.d_tag == DT_ALPHA_PLTRO && dyn.d_un.d_val != 0)
		      return false; /* This PLT should not be writable.  */
		  }
	      return pltgot == shdr->sh_addr;
	    }
	}
    }

  return false;
}

/* Check whether given symbol's st_value and st_size are OK despite failing
   normal checks.  */
bool
alpha_check_special_symbol (Elf *elf __attribute__ ((unused)),
			    const GElf_Sym *sym __attribute__ ((unused)),
			    const char *name,
			    const GElf_Shdr *destshdr __attribute__ ((unused)))
{
  if (name == NULL)
    return false;

  if (strcmp (name, "_GLOBAL_OFFSET_TABLE_") == 0)
    /* On Alpha any place in the section is valid.  */
    return true;

  return false;
}

/* Check whether only valid bits are set on the st_other symbol flag.
   Standard ST_VISIBILITY have already been masked off.  */
bool
alpha_check_st_other_bits (unsigned char st_other)
{
  return ((((st_other & STO_ALPHA_STD_GPLOAD) == STO_ALPHA_NOPV)
	   || ((st_other & STO_ALPHA_STD_GPLOAD) == STO_ALPHA_STD_GPLOAD))
	  && (st_other &~ STO_ALPHA_STD_GPLOAD) == 0);
}
