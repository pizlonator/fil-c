/* Get information from a source line record returned by libdwfl.
   Copyright (C) 2010 Red Hat, Inc.
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

#include "libdwflP.h"
#include "libdwP.h"

Dwarf_Line *
dwfl_dwarf_line (Dwfl_Line *line, Dwarf_Addr *bias)
{
  if (line == NULL)
    return NULL;

  struct dwfl_cu *cu = dwfl_linecu (line);
  const Dwarf_Line *info = &cu->die.cu->lines->info[line->idx];

  *bias = dwfl_adjusted_dwarf_addr (cu->mod, 0);
  return (Dwarf_Line *) info;
}
