/* Register names and numbers for PowerPC DWARF.
   Copyright (C) 2005, 2006, 2007 Red Hat, Inc.
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

ssize_t
ppc_register_info (Ebl *ebl __attribute__ ((unused)),
		   int regno, char *name, size_t namelen,
		   const char **prefix, const char **setname,
		   int *bits, int *type)
{
  if (name == NULL)
    return 1156;

  if (regno < 0 || regno > 1155 || namelen < 8)
    return -1;

  *prefix = "";
  *bits = ebl->machine == EM_PPC64 ? 64 : 32;
  *type = (regno < 32 ? DW_ATE_signed
	   : regno < 64 ? DW_ATE_float : DW_ATE_unsigned);

  if (regno < 32 || regno == 64 || regno == 66)
    *setname = "integer";
  else if (regno < 64 || regno == 65)
    {
      *setname = "FPU";
      if (ebl->machine != EM_PPC64 && regno < 64)
	*bits = 64;
    }
  else if (regno == 67 || regno == 356 || regno == 612 || regno >= 1124)
    {
      *setname = "vector";
      *bits = regno >= 1124 ? 128 : 32;
    }
  else
    *setname = "privileged";

  switch (regno)
    {
    case 0 ... 9:
      name[0] = 'r';
      name[1] = regno + '0';
      namelen = 2;
      break;

    case 10 ... 31:
      name[0] = 'r';
      name[1] = regno / 10 + '0';
      name[2] = regno % 10 + '0';
      namelen = 3;
      break;

    case 32 + 0 ... 32 + 9:
      name[0] = 'f';
      name[1] = (regno - 32) + '0';
      namelen = 2;
      break;

    case 32 + 10 ... 32 + 31:
      name[0] = 'f';
      name[1] = (regno - 32) / 10 + '0';
      name[2] = (regno - 32) % 10 + '0';
      namelen = 3;
      break;

    case 64:
      return stpcpy (name, "cr") + 1 - name;
    case 65:
      return stpcpy (name, "fpscr") + 1 - name;
    case 66:
      return stpcpy (name, "msr") + 1 - name;
    case 67:			/* XXX unofficial assignment */
      return stpcpy (name, "vscr") + 1 - name;

    case 70 + 0 ... 70 + 9:
      name[0] = 's';
      name[1] = 'r';
      name[2] = (regno - 70) + '0';
      namelen = 3;
      break;

    case 70 + 10 ... 70 + 15:
      name[0] = 's';
      name[1] = 'r';
      name[2] = (regno - 70) / 10 + '0';
      name[3] = (regno - 70) % 10 + '0';
      namelen = 4;
      break;

    case 101:
      return stpcpy (name, "xer") + 1 - name;
    case 108:
      return stpcpy (name, "lr") + 1 - name;
    case 109:
      return stpcpy (name, "ctr") + 1 - name;
    case 118:
      return stpcpy (name, "dsisr") + 1 - name;
    case 119:
      return stpcpy (name, "dar") + 1 - name;
    case 122:
      return stpcpy (name, "dec") + 1 - name;
    case 356:
      return stpcpy (name, "vrsave") + 1 - name;
    case 612:
      return stpcpy (name, "spefscr") + 1 - name;
    case 100:
      if (*bits == 32)
	return stpcpy (name, "mq") + 1 - name;
      FALLTHROUGH;
    case 102 ... 107:
      name[0] = 's';
      name[1] = 'p';
      name[2] = 'r';
      name[3] = (regno - 100) + '0';
      namelen = 4;
      break;

    case 114:
      return stpcpy (name, "tfhar") + 1 - name;
    case 115:
      return stpcpy (name, "tfiar") + 1 - name;
    case 116:
      return stpcpy (name, "texasr") + 1 - name;

    case 110 ... 113:
    case 117:
    case 120 ... 121:
    case 123 ... 199:
      name[0] = 's';
      name[1] = 'p';
      name[2] = 'r';
      name[3] = (regno - 100) / 10 + '0';
      name[4] = (regno - 100) % 10 + '0';
      namelen = 5;
      break;

    case 200 ... 355:
    case 357 ... 611:
    case 613 ... 999:
      name[0] = 's';
      name[1] = 'p';
      name[2] = 'r';
      name[3] = (regno - 100) / 100 + '0';
      name[4] = ((regno - 100) % 100 / 10) + '0';
      name[5] = (regno - 100) % 10 + '0';
      namelen = 6;
      break;

    case 1124 + 0 ... 1124 + 9:
      name[0] = 'v';
      name[1] = 'r';
      name[2] = (regno - 1124) + '0';
      namelen = 3;
      break;

    case 1124 + 10 ... 1124 + 31:
      name[0] = 'v';
      name[1] = 'r';
      name[2] = (regno - 1124) / 10 + '0';
      name[3] = (regno - 1124) % 10 + '0';
      namelen = 4;
      break;

    default:
      *setname = NULL;
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}

__typeof (ppc_register_info)
     ppc64_register_info __attribute__ ((alias ("ppc_register_info")));
