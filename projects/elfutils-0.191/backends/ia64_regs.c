/* Register names and numbers for IA64 DWARF.
   Copyright (C) 2006 Red Hat, Inc.
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
ia64_register_info (Ebl *ebl __attribute__ ((unused)),
		    int regno, char *name, size_t namelen,
		    const char **prefix, const char **setname,
		    int *bits, int *type)
{
  if (name == NULL)
    return 687 + 64;

  if (regno < 0 || regno > 687 + 63 || namelen < 12)
    return -1;

  *prefix = "ar.";
  *setname = "application";
  *bits = 64;
  *type = DW_ATE_signed;
  switch (regno)
    {
    case 0 ... 9:
      name[0] = 'r';
      name[1] = (regno - 0) + '0';
      namelen = 2;
      *setname = "integer";
      *prefix = "";
      break;

    case 10 ... 99:
      name[0] = 'r';
      name[1] = (regno - 0) / 10 + '0';
      name[2] = (regno - 0) % 10 + '0';
      namelen = 3;
      *setname = "integer";
      *prefix = "";
      break;

    case 100 ... 127:
      name[0] = 'r';
      name[1] = '1';
      name[2] = (regno - 100) / 10 + '0';
      name[3] = (regno - 0) % 10 + '0';
      namelen = 4;
      *setname = "integer";
      *prefix = "";
      break;

    case 128 + 0 ... 128 + 9:
      name[0] = 'f';
      name[1] = (regno - 128) + '0';
      namelen = 2;
      *type = DW_ATE_float;
      *bits = 128;
      *setname = "FPU";
      *prefix = "";
      break;

    case 128 + 10 ... 128 + 99:
      name[0] = 'f';
      name[1] = (regno - 128) / 10 + '0';
      name[2] = (regno - 128) % 10 + '0';
      namelen = 3;
      *setname = "FPU";
      *prefix = "";
      break;

    case 128 + 100 ... 128 + 127:
      name[0] = 'f';
      name[1] = '1';
      name[2] = (regno - 128 - 100) / 10 + '0';
      name[3] = (regno - 128) % 10 + '0';
      namelen = 4;
      *type = DW_ATE_float;
      *bits = 128;
      *setname = "FPU";
      *prefix = "";
      break;

    case 320 + 0 ... 320 + 7:
      name[0] = 'b';
      name[1] = (regno - 320) + '0';
      namelen = 2;
      *type = DW_ATE_address;
      *setname = "branch";
      *prefix = "";
      break;

    case 328 ... 333:
      {
	static const char named_special[][5] =
	  {
	    "vfp", "vrap", "pr", "ip", "psr", "cfm"
	  };
	*setname = "special";
	*prefix = "";
	*type = regno == 331 ? DW_ATE_address : DW_ATE_unsigned;
	return stpcpy (name, named_special[regno - 328]) + 1 - name;
      }

    case 590:
      *setname = "special";
      *prefix = "";
      *type = DW_ATE_unsigned;
      return stpcpy (name, "bof") + 1 - name;

    case 334 + 0 ... 334 + 7:
      name[0] = 'k';
      name[1] = 'r';
      name[2] = (regno - 334) + '0';
      namelen = 3;
      *prefix = "";
      break;

    case 334 + 8 ... 334 + 127:
      {
	static const char named_ar[][9] =
	  {
	    [16 - 8] = "rsc",
	    [17 - 8] = "bsp",
	    [18 - 8] = "bspstore",
	    [19 - 8] = "rnat",
	    [21 - 8] = "fcr",
	    [24 - 8] = "eflag",
	    [25 - 8] = "csd",
	    [26 - 8] = "ssd",
	    [27 - 8] = "cflg",
	    [28 - 8] = "fsr",
	    [29 - 8] = "fir",
	    [30 - 8] = "fdr",
	    [32 - 8] = "ccv",
	    [36 - 8] = "unat",
	    [40 - 8] = "fpsr",
	    [44 - 8] = "itc",
	    [64 - 8] = "pfs",
	    [65 - 8] = "lc",
	    [66 - 8] = "ec",
	  };
	const size_t idx = regno - (334 + 8);
	*type = DW_ATE_unsigned;
	if (idx == 1 || idx == 2)
	  *type = DW_ATE_address;
	if (idx < sizeof named_ar / sizeof named_ar[0]
	    && named_ar[idx][0] != '\0')
	  return stpcpy (name, named_ar[idx]) + 1 - name;

	name[0] = 'a';
	name[1] = 'r';
	switch (regno - 334)
	  {
	  case 0 ... 9:
	    name[2] = (regno - 334) + '0';
	    namelen = 3;
	    break;
	  case 10 ... 99:
	    name[2] = (regno - 334) / 10 + '0';
	    name[3] = (regno - 334) % 10 + '0';
	    namelen = 4;
	    break;
	  case 100 ... 127:
	    name[2] = '1';
	    name[3] = (regno - 334 - 100) / 10 + '0';
	    name[4] = (regno - 334) % 10 + '0';
	    namelen = 5;
	    break;
	  }
	*prefix = "";
	break;
      }

    case 462 + 0 ... 462 + 9:
      name[0] = 'n';
      name[1] = 'a';
      name[2] = 't';
      name[3] = (regno - 462) + '0';
      namelen = 4;
      *setname = "NAT";
      *type = DW_ATE_boolean;
      *bits = 1;
      *prefix = "";
      break;

    case 462 + 10 ... 462 + 99:
      name[0] = 'n';
      name[1] = 'a';
      name[2] = 't';
      name[3] = (regno - 462) / 10 + '0';
      name[4] = (regno - 462) % 10 + '0';
      namelen = 5;
      *setname = "NAT";
      *type = DW_ATE_boolean;
      *bits = 1;
      *prefix = "";
      break;

    case 462 + 100 ... 462 + 127:
      name[0] = 'n';
      name[1] = 'a';
      name[2] = 't';
      name[3] = '1';
      name[4] = (regno - 462 - 100) / 10 + '0';
      name[5] = (regno - 462) % 10 + '0';
      namelen = 6;
      *setname = "NAT";
      *type = DW_ATE_boolean;
      *bits = 1;
      *prefix = "";
      break;

    case 687 + 0 ... 687 + 9:
      name[0] = 'p';
      name[1] = (regno - 687) + '0';
      namelen = 2;
      *setname = "predicate";
      *type = DW_ATE_boolean;
      *bits = 1;
      *prefix = "";
      break;

    case 687 + 10 ... 687 + 63:
      name[0] = 'p';
      name[1] = (regno - 687) / 10 + '0';
      name[2] = (regno - 687) % 10 + '0';
      namelen = 3;
      *setname = "predicate";
      *type = DW_ATE_boolean;
      *bits = 1;
      *prefix = "";
      break;

    default:
      *setname = NULL;
      return 0;
    }

  name[namelen++] = '\0';
  return namelen;
}
