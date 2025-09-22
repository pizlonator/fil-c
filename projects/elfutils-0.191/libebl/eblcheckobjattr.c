/* Check object attributes.
   Copyright (C) 2008 Red Hat, Inc.
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
#include <libeblP.h>


bool
ebl_check_object_attribute (Ebl *ebl, const char *vendor, int tag,
			    uint64_t value, const char **tag_name,
			    const char **value_name)
{
  if (ebl->check_object_attribute (ebl, vendor, tag, value,
				   tag_name, value_name))
    return true;

  if (strcmp (vendor, "gnu"))
    return false;

  if (tag == 32)
    {
      *tag_name = "compatibility";
      return true;
    }

  return false;
}
