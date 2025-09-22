/* i386 specific auxv handling.
   Copyright (C) 2007 Red Hat, Inc.
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

#define BACKEND i386_
#include "libebl_CPU.h"

int
EBLHOOK(auxv_info) (GElf_Xword a_type, const char **name, const char **format)
{
  if (a_type != AT_HWCAP)
    return 0;

  *name = "HWCAP";
  *format = "b"
    "fpu\0" "vme\0" "de\0" "pse\0" "tsc\0" "msr\0" "pae\0" "mce\0"
    "cx8\0" "apic\0" "10\0" "sep\0" "mtrr\0" "pge\0" "mca\0" "cmov\0"
    "pat\0" "pse36\0" "pn\0" "clflush\0" "20\0" "dts\0" "acpi\0" "mmx\0"
    "fxsr\0" "sse\0" "sse2\0" "ss\0" "ht\0" "tm\0" "ia64\0" "pbe\0" "\0";
  return 1;
}

__typeof (i386_auxv_info) x86_64_auxv_info
			  __attribute__ ((alias ("i386_auxv_info")));
