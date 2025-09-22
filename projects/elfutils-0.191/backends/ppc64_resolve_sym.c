/* Resolve symbol values through .opd function descriptors.
   Copyright (C) 2013 Red Hat, Inc.
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

#define BACKEND ppc64_
#include "libebl_CPU.h"

/* Resolve a function descriptor if addr points into the .opd section.
   The .opd section contains function descriptors consisting of 3 addresses.
   function, toc and chain. We are just interested in the first.
   http://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi-1.9.html#FUNC-DES

   Returns true if the given address could be resolved, false otherwise.
*/
bool
ppc64_resolve_sym_value (Ebl *ebl, GElf_Addr *addr)
{
  if (ebl->fd_data != NULL && *addr >= ebl->fd_addr
      && *addr + sizeof (Elf64_Addr) <= ebl->fd_addr + ebl->fd_data->d_size)
    {
      GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (ebl->elf, &ehdr_mem);
      if (ehdr != NULL)
	{
	  Elf_Data opd_in, opd_out;
	  opd_in.d_buf = ebl->fd_data->d_buf + (*addr - ebl->fd_addr);
	  opd_out.d_buf = addr;
	  opd_out.d_size = opd_in.d_size = sizeof (Elf64_Addr);
	  opd_out.d_type = opd_in.d_type = ELF_T_ADDR;
	  if (elf64_xlatetom (&opd_out, &opd_in,
			      ehdr->e_ident[EI_DATA]) != NULL)
	    return true;
	}
    }
  return false;
}
