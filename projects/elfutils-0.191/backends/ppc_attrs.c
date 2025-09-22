/* Object attribute tags for PowerPC.
   Copyright (C) 2008, 2009 Red Hat, Inc.
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
#include <dwarf.h>

#define BACKEND ppc_
#include "libebl_CPU.h"

bool
ppc_check_object_attribute (Ebl *ebl __attribute__ ((unused)),
			    const char *vendor, int tag, uint64_t value,
			    const char **tag_name, const char **value_name)
{
  if (!strcmp (vendor, "gnu"))
    switch (tag)
      {
      case 4:
	*tag_name = "GNU_Power_ABI_FP";
	static const char *fp_kinds[] =
	  {
	    "Hard or soft float",
	    "Hard float",
	    "Soft float",
	    "Single-precision hard float",
	  };
	if (value < sizeof fp_kinds / sizeof fp_kinds[0])
	  *value_name = fp_kinds[value];
	return true;

      case 8:
	*tag_name = "GNU_Power_ABI_Vector";
	static const char *vector_kinds[] =
	  {
	    "Any", "Generic", "AltiVec", "SPE"
	  };
	if (value < sizeof vector_kinds / sizeof vector_kinds[0])
	  *value_name = vector_kinds[value];
	return true;

      case 12:
	*tag_name = "GNU_Power_ABI_Struct_Return";
	static const char *struct_return_kinds[] =
	  {
	    "Any", "r3/r4", "Memory"
	  };
	if (value < sizeof struct_return_kinds / sizeof struct_return_kinds[0])
	  *value_name = struct_return_kinds[value];
	return true;
      }

  return false;
}

__typeof (ppc_check_object_attribute)
     ppc64_check_object_attribute
     __attribute__ ((alias ("ppc_check_object_attribute")));
