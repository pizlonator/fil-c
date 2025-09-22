/* Register names and numbers for RISC-V DWARF.
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

#define BACKEND riscv_
#include "libebl_CPU.h"

ssize_t
riscv_register_info (Ebl *ebl, int regno, char *name, size_t namelen,
		     const char **prefix, const char **setname,
		     int *bits, int *type)
{
  if (name == NULL)
    return 64;

  *prefix = "";

  if (regno < 32)
    {
      *setname = "integer";
      *type = DW_ATE_signed;
      *bits = ebl->class == ELFCLASS64 ? 64 : 32;
    }
  else
    {
      *setname = "FPU";
      *type = DW_ATE_float;
      *bits = 64;
    }

  switch (regno)
    {
    case 0:
      return stpcpy (name, "zero") + 1 - name;

    case 1:
      *type = DW_ATE_address;
      return stpcpy (name, "ra") + 1 - name;

    case 2:
      *type = DW_ATE_address;
      return stpcpy (name, "sp") + 1 - name;

    case 3:
      *type = DW_ATE_address;
      return stpcpy (name, "gp") + 1 - name;

    case 4:
      *type = DW_ATE_address;
      return stpcpy (name, "tp") + 1 - name;

    case 5 ... 7:
      name[0] = 't';
      name[1] = regno - 5 + '0';
      namelen = 2;
      break;

    case 8 ... 9:
      name[0] = 's';
      name[1] = regno - 8 + '0';
      namelen = 2;
      break;

    case 10 ... 17:
      name[0] = 'a';
      name[1] = regno - 10 + '0';
      namelen = 2;
      break;

    case 18 ... 25:
      name[0] = 's';
      name[1] = regno - 18 + '2';
      namelen = 2;
      break;

    case 26 ... 27:
      name[0] = 's';
      name[1] = '1';
      name[2] = regno - 26 + '0';
      namelen = 3;
      break;

    case 28 ... 31:
      name[0] = 't';
      name[1] = regno - 28 + '3';
      namelen = 2;
      break;

    case 32 ... 39:
      name[0] = 'f';
      name[1] = 't';
      name[2] = regno - 32 + '0';
      namelen = 3;
      break;

    case 40 ... 41:
      name[0] = 'f';
      name[1] = 's';
      name[2] = regno - 40 + '0';
      namelen = 3;
      break;

    case 42 ... 49:
      name[0] = 'f';
      name[1] = 'a';
      name[2] = regno - 42 + '0';
      namelen = 3;
      break;

    case 50 ... 57:
      name[0] = 'f';
      name[1] = 's';
      name[2] = regno - 50 + '2';
      namelen = 3;
      break;

    case 58 ... 59:
      name[0] = 'f';
      name[1] = 's';
      name[2] = '1';
      name[3] = regno - 58 + '0';
      namelen = 4;
      break;

    case 60 ... 61:
      name[0] = 'f';
      name[1] = 't';
      name[2] = regno - 60 + '8';
      namelen = 3;
      break;

    case 62 ... 63:
      name[0] = 'f';
      name[1] = 't';
      name[2] = '1';
      name[3] = regno - 62 + '0';
      namelen = 4;
      break;

    default:
      *setname = NULL;
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
