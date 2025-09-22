/* Get return address register for frame.
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

#include "cfi.h"

int
dwarf_frame_info (Dwarf_Frame *fs, Dwarf_Addr *start, Dwarf_Addr *end,
		  bool *signalp)
{
  /* Maybe there was a previous error.  */
  if (fs == NULL)
    return -1;

  if (start != NULL)
    *start = fs->start;
  if (end != NULL)
    *end = fs->end;
  if (signalp != NULL)
    *signalp = fs->fde->cie->signal_frame;
  return fs->fde->cie->return_address_register;
}
