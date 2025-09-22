/* Frame cache handling.
   Copyright (C) 2009, 2015 Red Hat, Inc.
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

#include "libebl.h"
#include "cfi.h"
#include <search.h>
#include <stdlib.h>


static void
free_cie (void *arg)
{
  struct dwarf_cie *cie = arg;

  free ((Dwarf_Frame *) cie->initial_state);
  free (cie);
}

#define free_fde	free

static void
free_expr (void *arg)
{
  struct loc_s *loc = arg;

  free (loc->loc);
  free (loc);
}

void
internal_function
__libdw_destroy_frame_cache (Dwarf_CFI *cache)
{
  /* Most of the data is in our two search trees.  */
  tdestroy (cache->fde_tree, free_fde);
  tdestroy (cache->cie_tree, free_cie);
  tdestroy (cache->expr_tree, free_expr);

  if (cache->ebl != NULL && cache->ebl != (void *) -1l)
    ebl_closebackend (cache->ebl);
}
