/* m68k specific symbolic name handling.
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

#include <assert.h>
#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND m68k_
#include "libebl_CPU.h"


/* Return true if the symbol type is that referencing the GOT.  */
bool
m68k_gotpc_reloc_check (Elf *elf __attribute__ ((unused)), int type)
{
  switch (type)
    {
    case R_68K_GOT32:
    case R_68K_GOT16:
    case R_68K_GOT8:
      return true;
    }
  return false;
}

/* Check for the simple reloc types.  */
Elf_Type
m68k_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
			int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
    case R_68K_32:
      return ELF_T_SWORD;
    case R_68K_16:
      return ELF_T_HALF;
    case R_68K_8:
      return ELF_T_BYTE;
    default:
      return ELF_T_NUM;
    }
}
