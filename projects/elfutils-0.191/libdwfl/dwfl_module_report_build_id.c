/* Report build ID information for a module.
   Copyright (C) 2007, 2008 Red Hat, Inc.
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

// XXX vs report changed module: punting old file
int
dwfl_module_report_build_id (Dwfl_Module *mod,
			     const unsigned char *bits, size_t len,
			     GElf_Addr vaddr)
{
  if (mod == NULL)
    return -1;

  if (mod->main.elf != NULL)
    {
      /* Once we know about a file, we won't take any lies about
	 its contents.  The only permissible call is a no-op.  */

      if ((size_t) mod->build_id_len == len
	  && (mod->build_id_vaddr == vaddr || vaddr == 0)
	  && !memcmp (bits, mod->build_id_bits, len))
	return 0;

      __libdwfl_seterrno (DWFL_E_ALREADY_ELF);
      return -1;
    }

  if (vaddr != 0 && (vaddr < mod->low_addr || vaddr + len > mod->high_addr))
    {
      __libdwfl_seterrno (DWFL_E_ADDR_OUTOFRANGE);
      return -1;
    }

  void *copy = NULL;
  if (len > 0)
    {
      copy = malloc (len);
      if (unlikely (copy == NULL))
	{
	  __libdwfl_seterrno (DWFL_E_NOMEM);
	  return -1;
	}
      memcpy (copy, bits, len);
    }

  free (mod->build_id_bits);

  mod->build_id_bits = copy;
  mod->build_id_len = len;
  mod->build_id_vaddr = vaddr;

  return 0;
}
INTDEF (dwfl_module_report_build_id)
