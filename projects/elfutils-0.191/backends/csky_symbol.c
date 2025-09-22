/* C-SKY specific symbolic name handling.
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

#include <assert.h>
#include <elf.h>
#include <stddef.h>
#include <string.h>

#define BACKEND csky_
#include "libebl_CPU.h"

/* Check for the simple reloc types.  */
Elf_Type
csky_reloc_simple_type (Ebl *ebl __attribute__ ((unused)), int type,
			int *addsub __attribute__ ((unused)))
{
  switch (type)
    {
    case R_CKCORE_ADDR32:
      return ELF_T_WORD;
    default:
      return ELF_T_NUM;
    }
}

bool
csky_machine_flag_check (GElf_Word flags)
{
  switch (flags & EF_CSKY_ABIMASK)
    {
    case EF_CSKY_ABIV2:
      return true;
    case EF_CSKY_ABIV1:
    default:
      return false;
    }
}

const char *
csky_section_type_name (int type,
		       char *buf __attribute__ ((unused)),
		       size_t len __attribute__ ((unused)))
{
  if (type == SHT_CSKY_ATTRIBUTES)
    return "CSKY_ATTRIBUTES";

  return NULL;
}
