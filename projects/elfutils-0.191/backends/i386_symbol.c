/* i386 specific symbolic name handling.
   Copyright (C) 2000, 2001, 2002, 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <assert.h>
#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND i386_
#include "libebl_CPU.h"


/* Return true if the symbol type is that referencing the GOT.  */
bool
i386_gotpc_reloc_check (Elf *elf __attribute__ ((unused)), int type)
{
  return type == R_386_GOTPC;
}

/* Check for the simple reloc types.  */
Elf_Type
i386_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
			int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
    case R_386_32:
      return ELF_T_SWORD;
    case R_386_16:
      return ELF_T_HALF;
    case R_386_8:
      return ELF_T_BYTE;
    default:
      return ELF_T_NUM;
    }
}

/* Check section name for being that of a debug information section.  */
bool (*generic_debugscn_p) (const char *);
bool
i386_debugscn_p (const char *name)
{
  return (generic_debugscn_p (name)
	  || strcmp (name, ".stab") == 0
	  || strcmp (name, ".stabstr") == 0);
}
