/* C-SKY ABI-specified defaults for DWARF CFI.
   Copyright (C) 2019 Hangzhou C-SKY Microsystems co.,ltd.
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

#define BACKEND csky_
#include "libebl_CPU.h"

bool
csky_check_object_attribute (Ebl *ebl __attribute__ ((unused)),
			    const char *vendor, int tag,
			    uint64_t value __attribute__ ((unused)),
			    const char **tag_name,
			    const char **value_name __attribute__ ((unused)))
{
  if (!strcmp (vendor, "csky"))
    switch (tag)
      {
      case 4:
	*tag_name = "CSKY_ARCH_NAME";
	return true;

      case 5:
	*tag_name = "CSKY_CPU_NAME";
	return true;

      case 6:
        *tag_name = "CSKY_ISA_FLAGS";
        return true;

      case 7:
        *tag_name = "CSKY_ISA_EXT_FLAGS";
        return true;
      }

  return false;
}
