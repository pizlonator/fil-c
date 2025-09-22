/* Register names and numbers for M68K DWARF.
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

#include <stdio.h>
#include <string.h>
#include <dwarf.h>

#define BACKEND m68k_
#include "libebl_CPU.h"

ssize_t
m68k_register_info (Ebl *ebl __attribute__ ((unused)),
		    int regno, char *name, size_t namelen,
		    const char **prefix, const char **setname,
		    int *bits, int *type)
{
  if (name == NULL)
    return 25;

  if (regno < 0 || regno > 24 || namelen < 5)
    return -1;

  *prefix = "%";
  *setname = "integer";
  *bits = 32;

  switch (regno)
    {
    case 0 ... 7:
      *type = DW_ATE_signed;
      name[0] = 'd';
      name[1] = regno + '0';
      namelen = 2;
      break;

    case 8 ... 15:
      *type = DW_ATE_address;
      name[0] = 'a';
      name[1] = regno - 8 + '0';
      namelen = 2;
      break;

    case 16 ... 23:
      *type = DW_ATE_float;
      *setname = "FPU";
      *bits = 96;
      name[0] = 'f';
      name[1] = 'p';
      name[2] = regno - 16 + '0';
      namelen = 3;
      break;

    case 24:
      *type = DW_ATE_address;
      name[0] = 'p';
      name[1] = 'c';
      namelen = 2;
      break;

    /* Can't happen.  */
    default:
      *setname = NULL;
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
