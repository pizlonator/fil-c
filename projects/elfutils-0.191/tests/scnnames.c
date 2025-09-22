/* Copyright (C) 1998, 1999, 2000, 2002, 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int
main (int argc, char *argv[])
{
  Elf *elf;
  int fd;
  GElf_Ehdr ehdr;
  size_t strndx;
  Elf_Scn *scn;

  if (argc < 2)
    {
      puts ("missing parameter");
      exit (1);
    }

  fd = open (argv[1], O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open \"%s\": %s\n", argv[1], strerror (errno));
      exit (1);
    }

  elf_version (EV_CURRENT);

  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("cannot open ELF file: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (elf_kind (elf) != ELF_K_ELF)
    {
      printf ("\"%s\" is not an ELF file\n", argv[1]);
      exit (1);
    }

  if (gelf_getehdr (elf, &ehdr) == NULL)
    {
      printf ("cannot get the ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  strndx = ehdr.e_shstrndx;

  scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      char *name = NULL;
      GElf_Shdr shdr;

      if (gelf_getshdr (scn, &shdr) != NULL)
	name = elf_strptr (elf, strndx, (size_t) shdr.sh_name);

      printf ("section: `%s'\n", name);
    }

  if (elf_end (elf) != 0)
    {
      printf ("error while freeing ELF descriptor: %s\n", elf_errmsg (-1));
      exit (1);
    }

  return 0;
}
