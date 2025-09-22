/* Return size of GNU compressed section.
   Copyright (C) 2015 Red Hat, Inc.
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
#include "libelfP.h"

ssize_t
dwelf_scn_gnu_compressed_size (Elf_Scn *scn)
{
  if (scn == NULL)
    return -1;

  GElf_Shdr shdr;
  if (gelf_getshdr (scn, &shdr) == NULL)
    return -1;

  /* Allocated or no bits sections can never be compressed.  */
  if ((shdr.sh_flags & SHF_ALLOC) != 0
      || shdr.sh_type == SHT_NULL
      || shdr.sh_type == SHT_NOBITS)
    return -1;

  Elf_Data *d = elf_rawdata (scn, NULL);
  if (d == NULL)
    return -1;

  if (d->d_size >= 4 + 8
      && memcmp (d->d_buf, "ZLIB", 4) == 0)
    {
      /* There is a 12-byte header of "ZLIB" followed by
	 an 8-byte big-endian size.  There is only one type and
	 alignment isn't preserved separately.  */
      uint64_t size;
      memcpy (&size, d->d_buf + 4, sizeof size);
      size = be64toh (size);

      /* One more sanity check, size should be bigger than original
	 data size plus some overhead (4 chars ZLIB + 8 bytes size + 6
	 bytes zlib stream overhead + 5 bytes overhead max for one 16K
	 block) and should fit into a size_t.  */
      if (size + 4 + 8 + 6 + 5 < d->d_size || size > SIZE_MAX)
	return -1;

      return size;
    }

  return -1;
}
