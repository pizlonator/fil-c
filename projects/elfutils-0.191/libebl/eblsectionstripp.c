/* Check whether section can be stripped.
   Copyright (C) 2005, 2013 Red Hat, Inc.
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

#include <string.h>
#include "libeblP.h"


bool
ebl_section_strip_p (Ebl *ebl, const GElf_Shdr *shdr,
		     const char *name, bool remove_comment,
		     bool only_remove_debug)
{
  /* If only debug information should be removed check the name.  There
     is unfortunately no other way.  */
  if (unlikely (only_remove_debug))
    {
      if (ebl_debugscn_p (ebl, name))
	return true;

      if (shdr->sh_type == SHT_RELA || shdr->sh_type == SHT_REL)
	{
	  Elf_Scn *scn_l = elf_getscn (ebl->elf, (shdr)->sh_info);
	  GElf_Shdr shdr_mem_l;
	  GElf_Shdr *shdr_l = gelf_getshdr (scn_l, &shdr_mem_l);
	  if (shdr_l != NULL)
	    {
	      size_t shstrndx;
	      if (elf_getshdrstrndx (ebl->elf, &shstrndx) != 0)
		return false;
	      const char *s_l = elf_strptr (ebl->elf, shstrndx,
					    shdr_l->sh_name);
	      if (s_l != NULL && ebl_debugscn_p (ebl, s_l))
		return true;
	    }
	}

      return false;
    }

  return SECTION_STRIP_P (shdr, name, remove_comment);
}
