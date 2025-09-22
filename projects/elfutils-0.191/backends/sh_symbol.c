/* SH specific relocation handling.
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

#include <elf.h>
#include <stddef.h>

#define BACKEND sh_
#include "libebl_CPU.h"


/* Return true if the symbol type is that referencing the GOT.  */
bool
sh_gotpc_reloc_check (Elf *elf __attribute__ ((unused)), int type)
{
  return type == R_SH_GOTPC;
}

/* Check for the simple reloc types.  */
Elf_Type
sh_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
		      int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
    case R_SH_DIR32:
      return ELF_T_WORD;
    default:
      return ELF_T_NUM;
    }
}

/* Check whether machine flags are valid.  */
bool
sh_machine_flag_check (GElf_Word flags)
{
  switch (flags & EF_SH_MACH_MASK)
    {
    case EF_SH_UNKNOWN:
    case EF_SH1:
    case EF_SH2:
    case EF_SH3:
    case EF_SH_DSP:
    case EF_SH3_DSP:
    case EF_SH4AL_DSP:
    case EF_SH3E:
    case EF_SH4:
    case EF_SH2E:
    case EF_SH4A:
    case EF_SH2A:
    case EF_SH4_NOFPU:
    case EF_SH4A_NOFPU:
    case EF_SH4_NOMMU_NOFPU:
    case EF_SH2A_NOFPU:
    case EF_SH3_NOMMU:
    case EF_SH2A_SH4_NOFPU:
    case EF_SH2A_SH3_NOFPU:
    case EF_SH2A_SH4:
    case EF_SH2A_SH3E:
      break;
    default:
      return false;
    }

  return ((flags &~ (EF_SH_MACH_MASK)) == 0);
}
