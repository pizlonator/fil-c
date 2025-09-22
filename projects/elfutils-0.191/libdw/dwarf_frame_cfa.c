/* Get CFA expression for frame.
   Copyright (C) 2009-2010 Red Hat, Inc.
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

#include "cfi.h"
#include <dwarf.h>
#include <stdlib.h>

int
dwarf_frame_cfa (Dwarf_Frame *fs, Dwarf_Op **ops, size_t *nops)
{
  /* Maybe there was a previous error.  */
  if (fs == NULL)
    return -1;

  int result = 0;
  switch (fs->cfa_rule)
    {
    case cfa_undefined:
      *ops = NULL;
      *nops = 0;
      break;

    case cfa_offset:
      /* The Dwarf_Op was already fully initialized by execute_cfi.  */
      *ops = &fs->cfa_data.offset;
      *nops = 1;
      break;

    case cfa_expr:
      /* Parse the expression into internal form.  */
      result = __libdw_intern_expression
	(NULL, fs->cache->other_byte_order,
	 fs->cache->e_ident[EI_CLASS] == ELFCLASS32 ? 4 : 8, 4,
	 &fs->cache->expr_tree, &fs->cfa_data.expr, false, false,
	 ops, nops, IDX_debug_frame);
      break;

    case cfa_invalid:
      __libdw_seterrno (DWARF_E_INVALID_CFI);
      result = -1;
      break;

    default:
      abort ();
    }

  return result;
}
