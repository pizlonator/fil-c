/* Compute frame state at PC.
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
dwarf_cfi_addrframe (Dwarf_CFI *cache, Dwarf_Addr address, Dwarf_Frame **frame)
{
  /* Maybe there was a previous error.  */
  if (cache == NULL)
    return -1;

  struct dwarf_fde *fde = __libdw_find_fde (cache, address);
  if (fde == NULL)
    return -1;

  int error = __libdw_frame_at_address (cache, fde, address, frame);
  if (error != DWARF_E_NOERROR)
    {
      __libdw_seterrno (error);
      return -1;
    }
  return 0;
}
INTDEF (dwarf_cfi_addrframe)
