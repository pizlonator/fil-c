/* Get CFI from DWARF file.
   Copyright (C) 2009 Red Hat, Inc.
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

#include "libdwP.h"
#include "cfi.h"
#include <dwarf.h>

Dwarf_CFI *
dwarf_getcfi (Dwarf *dbg)
{
  if (dbg == NULL)
    return NULL;

  if (dbg->cfi == NULL && dbg->sectiondata[IDX_debug_frame] != NULL)
    {
      Dwarf_CFI *cfi = libdw_typed_alloc (dbg, Dwarf_CFI);

      cfi->dbg = dbg;
      cfi->data = (Elf_Data_Scn *) dbg->sectiondata[IDX_debug_frame];

      cfi->search_table = NULL;
      cfi->search_table_vaddr = 0;
      cfi->search_table_entries = 0;
      cfi->search_table_encoding = DW_EH_PE_omit;

      cfi->frame_vaddr = 0;
      cfi->textrel = 0;
      cfi->datarel = 0;

      cfi->e_ident = (unsigned char *) elf_getident (dbg->elf, NULL);

      GElf_Ehdr ehdr;
      gelf_getehdr (dbg->elf, &ehdr);
      cfi->e_machine = ehdr.e_machine;

      cfi->other_byte_order = dbg->other_byte_order;
      cfi->default_same_value = false;

      cfi->next_offset = 0;
      cfi->cie_tree = cfi->fde_tree = cfi->expr_tree = NULL;

      cfi->ebl = NULL;

      dbg->cfi = cfi;
    }

  return dbg->cfi;
}
INTDEF (dwarf_getcfi)
