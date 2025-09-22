/* Return build ID information for a module.
   Copyright (C) 2007-2010, 2014 Red Hat, Inc.
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

#include "libdwflP.h"

static int
found_build_id (Dwfl_Module *mod, bool set,
		const void *bits, int len, GElf_Addr vaddr)
{
  if (!set)
    /* When checking bits, we do not compare VADDR because the
       address found in a debuginfo file may not match the main
       file as modified by prelink.  */
    return 1 + (mod->build_id_len == len
		&& !memcmp (bits, mod->build_id_bits, len));

  void *copy = malloc (len);
  if (unlikely (copy == NULL))
    {
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return -1;
    }

  mod->build_id_bits = memcpy (copy, bits, len);
  mod->build_id_vaddr = vaddr;
  mod->build_id_len = len;
  return len;
}

int
internal_function
__libdwfl_find_build_id (Dwfl_Module *mod, bool set, Elf *elf)
{
  const void *build_id_bits;
  GElf_Addr build_id_elfaddr;
  int build_id_len;

  /* For mod == NULL use dwelf_elf_gnu_build_id directly.  */
  assert (mod != NULL);

  int result = __libdwfl_find_elf_build_id (mod, elf, &build_id_bits,
					    &build_id_elfaddr, &build_id_len);
  if (result <= 0)
    return result;

  GElf_Addr build_id_vaddr = build_id_elfaddr + (build_id_elfaddr != 0
						 ? mod->main_bias : 0);
  return found_build_id (mod, set, build_id_bits, build_id_len, build_id_vaddr);
}

NEW_VERSION (dwfl_module_build_id, ELFUTILS_0.138)
int
dwfl_module_build_id (Dwfl_Module *mod,
		      const unsigned char **bits, GElf_Addr *vaddr)
{
  if (mod == NULL)
    return -1;

  if (mod->build_id_len == 0 && mod->main.elf != NULL)
    {
      /* We have the file, but have not examined it yet.  */
      int result = __libdwfl_find_build_id (mod, true, mod->main.elf);
      if (result <= 0)
	{
	  mod->build_id_len = -1;	/* Cache negative result.  */
	  return result;
	}
    }

  if (mod->build_id_len <= 0)
    return 0;

  *bits = mod->build_id_bits;
  *vaddr = mod->build_id_vaddr;
  return mod->build_id_len;
}
NEW_INTDEF (dwfl_module_build_id)

#ifdef SYMBOL_VERSIONING
COMPAT_VERSION (dwfl_module_build_id, ELFUTILS_0.130, vaddr_at_end)

int
_compat_vaddr_at_end_dwfl_module_build_id (Dwfl_Module *mod,
					   const unsigned char **bits,
					   GElf_Addr *vaddr)
{
  int result = INTUSE(dwfl_module_build_id) (mod, bits, vaddr);
  if (result > 0)
    *vaddr += (result + 3) & -4;
  return result;
}
#endif
