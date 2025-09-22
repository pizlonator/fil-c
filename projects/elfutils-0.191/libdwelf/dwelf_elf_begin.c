/* Creates an ELF handle from a possibly compressed file descriptor.
   Copyright (C) 2018 Red Hat, Inc.
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

#include "libdwelfP.h"
#include "libdwflP.h"
#include "libelfP.h"

NEW_VERSION (dwelf_elf_begin, ELFUTILS_0.177)
Elf *
dwelf_elf_begin (int fd)
{
  Elf *elf = NULL;
  Dwfl_Error e = __libdw_open_elf (fd, &elf);
  if (e == DWFL_E_NOERROR)
    return elf;

  /* Elf wasn't usable.  Make sure there is a proper elf error
     message.  This is probably not the real error, because there is
     no good way to propagate errnos or decompression errors, but
     better than nothing.  */

  if (e != DWFL_E_LIBELF)
    {
      /* Force a bad ELF error.  */
      char badelf[EI_NIDENT] = { };
      Elf *belf = elf_memory (badelf, EI_NIDENT);
      elf32_getehdr (belf);
      elf_end (belf);
    }

  return NULL;
}
OLD_VERSION (dwelf_elf_begin, ELFUTILS_0.175)
