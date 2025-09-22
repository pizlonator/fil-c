/* Returns the file name and crc stored in the .gnu_debuglink if found.
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

const char *
dwelf_elf_gnu_debuglink (Elf *elf, GElf_Word *crc)
{
  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx) < 0)
    return NULL;

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
        return NULL;

      const char *name = elf_strptr (elf, shstrndx, shdr->sh_name);
      if (name == NULL)
        return NULL;

      if (!strcmp (name, ".gnu_debuglink"))
        break;
    }

  if (scn == NULL)
    return NULL;

  /* Found the .gnu_debuglink section.  Extract its contents.  */
  Elf_Data *rawdata = elf_rawdata (scn, NULL);
  if (rawdata == NULL || rawdata->d_buf == NULL)
    return NULL;

  /* The CRC comes after the zero-terminated file name,
     (aligned up to 4 bytes) at the end of the section data.  */
  if (rawdata->d_size <= sizeof *crc
      || memchr (rawdata->d_buf, '\0', rawdata->d_size - sizeof *crc) == NULL)
    return NULL;

  Elf_Data crcdata =
    {
      .d_type = ELF_T_WORD,
      .d_buf = crc,
      .d_size = sizeof *crc,
      .d_version = EV_CURRENT,
    };
  Elf_Data conv =
    {
      .d_type = ELF_T_WORD,
      .d_buf = rawdata->d_buf + rawdata->d_size - sizeof *crc,
      .d_size = sizeof *crc,
      .d_version = EV_CURRENT,
    };

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    return NULL;

  Elf_Data *d = gelf_xlatetom (elf, &crcdata, &conv, ehdr->e_ident[EI_DATA]);
  if (d == NULL)
    return NULL;
  assert (d == &crcdata);

  return rawdata->d_buf;
}
INTDEF(dwelf_elf_gnu_debuglink)
