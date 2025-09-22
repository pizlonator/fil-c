/* Returns the file name and build ID stored in the .gnu_altdebuglink if found.
   Copyright (C) 2014 Red Hat, Inc.
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

ssize_t
dwelf_dwarf_gnu_debugaltlink (Dwarf *dwarf,
			      const char **name_p,
			      const void **build_idp)
{
  Elf_Data *data = dwarf->sectiondata[IDX_gnu_debugaltlink];
  if (data == NULL)
    {
      return 0;
    }

  const void *ptr = memchr (data->d_buf, '\0', data->d_size);
  if (ptr == NULL)
    {
      __libdw_seterrno (DWARF_E_INVALID_ELF);
      return -1;
    }
  size_t build_id_len = data->d_size - (ptr - data->d_buf + 1);
  if (build_id_len == 0 || (size_t) (ssize_t) build_id_len != build_id_len)
    {
      __libdw_seterrno (DWARF_E_INVALID_ELF);
      return -1;
    }
  *name_p = data->d_buf;
  *build_idp = ptr + 1;
  return build_id_len;
}
INTDEF(dwelf_dwarf_gnu_debugaltlink)
