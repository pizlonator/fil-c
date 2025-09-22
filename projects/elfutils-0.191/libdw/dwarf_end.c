/* Release debugging handling context.
   Copyright (C) 2002-2011, 2014, 2018 Red Hat, Inc.
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

#include <search.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "libdwP.h"
#include "cfi.h"


static void
dwarf_package_index_free (Dwarf_Package_Index *index)
{
  if (index != NULL)
    {
      free (index->debug_info_offsets);
      free (index);
    }
}


static void
noop_free (void *arg __attribute__ ((unused)))
{
}


static void
cu_free (void *arg)
{
  struct Dwarf_CU *p = (struct Dwarf_CU *) arg;

  tdestroy (p->locs, noop_free);

  /* Only free the CU internals if its not a fake CU.  */
  if(p != p->dbg->fake_loc_cu && p != p->dbg->fake_loclists_cu
     && p != p->dbg->fake_addr_cu)
    {
      Dwarf_Abbrev_Hash_free (&p->abbrev_hash);

      /* Free split dwarf one way (from skeleton to split).  */
      if (p->unit_type == DW_UT_skeleton
	  && p->split != NULL && p->split != (void *)-1)
	{
	  /* The fake_addr_cu might be shared, only release one.  */
	  if (p->dbg->fake_addr_cu == p->split->dbg->fake_addr_cu)
	    p->split->dbg->fake_addr_cu = NULL;
	  /* There is only one DWP file. We free it later.  */
	  if (p->split->dbg != p->dbg->dwp_dwarf)
	    INTUSE(dwarf_end) (p->split->dbg);
	}
    }
}


int
dwarf_end (Dwarf *dwarf)
{
  if (dwarf != NULL)
    {
      dwarf_package_index_free (dwarf->tu_index);
      dwarf_package_index_free (dwarf->cu_index);

      if (dwarf->cfi != NULL)
	/* Clean up the CFI cache.  */
	__libdw_destroy_frame_cache (dwarf->cfi);

      Dwarf_Sig8_Hash_free (&dwarf->sig8_hash);

      /* The search tree for the CUs.  NB: the CU data itself is
	 allocated separately, but the abbreviation hash tables need
	 to be handled.  */
      tdestroy (dwarf->cu_tree, cu_free);
      tdestroy (dwarf->tu_tree, cu_free);

      /* Search tree for macro opcode tables.  */
      tdestroy (dwarf->macro_ops, noop_free);

      /* Search tree for decoded .debug_lines units.  */
      tdestroy (dwarf->files_lines, noop_free);

      /* And the split Dwarf.  */
      tdestroy (dwarf->split_tree, noop_free);

      /* Free the internally allocated memory.  */
      for (size_t i = 0; i < dwarf->mem_stacks; i++)
        {
          struct libdw_memblock *memp = dwarf->mem_tails[i];
          while (memp != NULL)
	    {
	      struct libdw_memblock *prevp = memp->prev;
	      free (memp);
	      memp = prevp;
	    }
        }
      if (dwarf->mem_tails != NULL)
        free (dwarf->mem_tails);
      pthread_rwlock_destroy (&dwarf->mem_rwl);

      /* Free the pubnames helper structure.  */
      free (dwarf->pubnames_sets);

      /* Free the ELF descriptor if necessary.  */
      if (dwarf->free_elf)
	elf_end (dwarf->elf);

      /* Free the fake location list CU.  */
      if (dwarf->fake_loc_cu != NULL)
	{
	  cu_free (dwarf->fake_loc_cu);
	  free (dwarf->fake_loc_cu);
	}
      if (dwarf->fake_loclists_cu != NULL)
	{
	  cu_free (dwarf->fake_loclists_cu);
	  free (dwarf->fake_loclists_cu);
	}
      if (dwarf->fake_addr_cu != NULL)
	{
	  cu_free (dwarf->fake_addr_cu);
	  free (dwarf->fake_addr_cu);
	}

      /* Did we find and allocate the alt Dwarf ourselves?  */
      if (dwarf->alt_fd != -1)
	{
	  INTUSE(dwarf_end) (dwarf->alt_dwarf);
	  close (dwarf->alt_fd);
	}

      if (dwarf->dwp_fd != -1)
	{
	  INTUSE(dwarf_end) (dwarf->dwp_dwarf);
	  close (dwarf->dwp_fd);
	}

      /* The cached path and dir we found the Dwarf ELF file in.  */
      free (dwarf->elfpath);
      free (dwarf->debugdir);

      /* Free the context descriptor.  */
      free (dwarf);
    }

  return 0;
}
INTDEF(dwarf_end)
