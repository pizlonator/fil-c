/* Return flag represented by attribute.
   Copyright (C) 2004-2009 Red Hat, Inc.
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

#include <dwarf.h>
#include "libdwP.h"


int
dwarf_formflag (Dwarf_Attribute *attr, bool *return_bool)
{
  if (attr == NULL)
    return -1;

  if (attr->form == DW_FORM_flag_present)
    {
      *return_bool = true;
      return 0;
    }

  if (unlikely (attr->form != DW_FORM_flag))
    {
      __libdw_seterrno (DWARF_E_NO_FLAG);
      return -1;
    }

  *return_bool = *attr->valp != 0;

  return 0;
}
