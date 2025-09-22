/* Test program for dwarf_next_cfi
   Copyright (C) 2018 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <assert.h>
#include <inttypes.h>
#include ELFUTILS_HEADER(dw)
#include <dwarf.h>
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "system.h"

void
handle_section (char *name, const unsigned char e_ident[],
		Elf_Scn *scn, const bool is_eh)
{
  if (is_eh)
    printf (".eh_frame\n");
  else
    printf (".debug_frame\n");

  GElf_Shdr mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &mem);
  if (shdr == NULL)
    error (EXIT_FAILURE, 0, "Couldn't get section header: %s",
	   elf_errmsg (-1));
  if ((shdr->sh_flags & SHF_COMPRESSED) != 0)
    {
      if (elf_compress (scn, 0, 0) < 0)
	error (EXIT_FAILURE, 0, "Couldn't decompress section: %s",
	       elf_errmsg (-1));
    }
  else if (name[0] == '.' && name[1] == 'z')
    {
      if (elf_compress_gnu (scn, 0, 0) < 0)
	error (EXIT_FAILURE, 0, "Couldn't decompress section: %s",
	       elf_errmsg (-1));
    }

  Elf_Data *data = elf_getdata (scn, NULL);
  if (data == NULL || data->d_buf == NULL)
    error (EXIT_FAILURE, 0, "no section data");

  int res;
  Dwarf_Off off;
  Dwarf_Off next_off = 0;
  Dwarf_CFI_Entry entry;
  while ((res = dwarf_next_cfi (e_ident, data, is_eh, off = next_off,
				&next_off, &entry)) == 0)
    {
      printf ("[%" PRId64 "] ", off);
      if (dwarf_cfi_cie_p (&entry))
	printf ("CIE augmentation=\"%s\"\n", entry.cie.augmentation);
      else
	{
	  printf ("FDE cie=[%" PRId64 "]\n", entry.fde.CIE_pointer);

	  Dwarf_Off cie_off = entry.fde.CIE_pointer;
	  Dwarf_Off cie_off_next;
	  Dwarf_CFI_Entry cie_entry;
	  if (dwarf_next_cfi (e_ident, data, is_eh, cie_off, &cie_off_next,
			      &cie_entry) != 0
	      || !dwarf_cfi_cie_p (&cie_entry))
	    error (EXIT_FAILURE, 0, "FDE doesn't point to CIE");
	}
    }

  if (res < 0)
    error (EXIT_FAILURE, 0, "dwarf_next_cfi failed: %s\n",
	   dwarf_errmsg (-1));
}

int
main (int argc, char *argv[])
{
  if (argc != 2)
    error (EXIT_FAILURE, 0, "need file name argument");

  const char *file = argv[1];
  printf ("%s\n", file);

  int fd = open (file, O_RDONLY);
  if (fd == -1)
    error (EXIT_FAILURE, errno, "cannot open input file `%s'", file);

  elf_version (EV_CURRENT);

  Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    error (EXIT_FAILURE, 0, "cannot create ELF descriptor: %s",
	   elf_errmsg (-1));

  size_t esize;
  const unsigned char *ident = (const unsigned char *) elf_getident (elf,
								     &esize);
  if (ident == NULL || esize < EI_NIDENT)
    error (EXIT_FAILURE, 0, "no, or too small, ELF ident");

  GElf_Ehdr ehdr;
  if (gelf_getehdr (elf, &ehdr) == NULL)
    error (EXIT_FAILURE, 0, "cannot get the ELF header: %s\n",
	   elf_errmsg (-1));

  size_t strndx = ehdr.e_shstrndx;

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr;
      if (gelf_getshdr (scn, &shdr) != NULL)
	{
	  char *name = elf_strptr (elf, strndx, (size_t) shdr.sh_name);
	  if (name != NULL && shdr.sh_type == SHT_PROGBITS)
	    {
	      if (strcmp (name, ".eh_frame") == 0)
		handle_section (name, ident, scn, true);
	      if (strcmp (name, ".debug_frame") == 0
		  || strcmp (name, ".zdebug_frame") == 0)
		handle_section (name, ident, scn, false);
	    }
	}
    }

  elf_end (elf);
  close (fd);

  return 0;
}
