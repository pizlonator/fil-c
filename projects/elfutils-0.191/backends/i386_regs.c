/* Register names and numbers for i386 DWARF.
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

#define BACKEND i386_
#include "libebl_CPU.h"

ssize_t
i386_register_info (Ebl *ebl __attribute__ ((unused)),
		    int regno, char *name, size_t namelen,
		    const char **prefix, const char **setname,
		    int *bits, int *type)
{
  if (name == NULL)
    return 46;

  if (regno < 0 || regno > 45 || namelen < 6)
    return -1;

  *prefix = "%";
  *bits = 32;
  *type = DW_ATE_unsigned;
  if (regno < 11)
    {
      *setname = "integer";
      if (regno < 9)
	*type = DW_ATE_signed;
    }
  else if (regno < 19)
    {
      *setname = "x87";
      *type = DW_ATE_float;
      *bits = 80;
    }
  else if (regno < 29)
    {
      *setname = "SSE";
      *bits = 128;
    }
  else if (regno < 37)
    {
      *setname = "MMX";
      *bits = 64;
    }
  else if (regno < 40)
    *setname = "FPU-control";
  else
    {
      *setname = "segment";
      *bits = 16;
    }

  switch (regno)
    {
      static const char baseregs[][2] =
	{
	  "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "ip"
	};

    case 4:
    case 5:
    case 8:
      *type = DW_ATE_address;
      FALLTHROUGH;
    case 0 ... 3:
    case 6 ... 7:
      name[0] = 'e';
      name[1] = baseregs[regno][0];
      name[2] = baseregs[regno][1];
      namelen = 3;
      break;

    case 9:
      return stpcpy (name, "eflags") + 1 - name;
    case 10:
      return stpcpy (name, "trapno") + 1 - name;

    case 11 ... 18:
      name[0] = 's';
      name[1] = 't';
      name[2] = regno - 11 + '0';
      namelen = 3;
      break;

    case 21 ... 28:
      name[0] = 'x';
      name[1] = 'm';
      name[2] = 'm';
      name[3] = regno - 21 + '0';
      namelen = 4;
      break;

    case 29 ... 36:
      name[0] = 'm';
      name[1] = 'm';
      name[2] = regno - 29 + '0';
      namelen = 3;
      break;

    case 37:
      *bits = 16;
      return stpcpy (name, "fctrl") + 1 - name;
    case 38:
      *bits = 16;
      return stpcpy (name, "fstat") + 1 - name;
    case 39:
      return stpcpy (name, "mxcsr") + 1 - name;

    case 40 ... 45:
      name[0] = "ecsdfg"[regno - 40];
      name[1] = 's';
      namelen = 2;
      break;

    default:
      *setname = NULL;
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
