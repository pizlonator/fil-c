/* Copyright (C) 2012 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdbool.h>
#include <inttypes.h>

int
main (int argc, char *argv[])
{
  if (argc != 3)
    {
      fprintf (stderr, "Needs two arguments.\n");
      fprintf (stderr, "First needs to be 'READ', 'MMAP' or 'FDREAD'\n");
      fprintf (stderr, "Second is the ELF file to read.\n");
      exit (2); /* user error */
    }

  bool do_mmap = false;
  bool close_fd = false;
  if (strcmp (argv[1], "READ") == 0)
    {
      do_mmap = false;
      close_fd = false;
    }
  else if (strcmp (argv[1], "MMAP") == 0)
    {
      do_mmap = true;
      close_fd = false;
    }
  else if  (strcmp (argv[1], "FDREAD") == 0)
    {
      do_mmap = false;
      close_fd = true;
    }
  else
    {
      fprintf (stderr, "First arg needs to be 'READ', 'MMAP' or 'FDREAD'\n");
      exit (2); /* user error */
    }

  elf_version (EV_CURRENT);

  int fd = open (argv[2], O_RDONLY);
  if (fd < 0)
    {
      fprintf (stderr, "Cannot open input file %s: %s\n", argv[2],
	       strerror (errno));
      exit (2);
    }

  Elf *elf = elf_begin (fd, do_mmap ? ELF_C_READ_MMAP : ELF_C_READ, NULL);
  if (elf == NULL)
    {
      fprintf (stderr, "elf_begin failed for %s: %s\n", argv[2],
	       elf_errmsg (-1));
      exit (2);
    }

  if (! do_mmap && close_fd)
    {
      if (elf_cntl (elf, ELF_C_FDREAD) < 0)
	{
	  fprintf (stderr, "elf_cntl failed for %s: %s\n", argv[2],
		   elf_errmsg (-1));
	  exit (1);
	}
      close (fd);
    }

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      printf ("Section at offset %#0" PRIx64 "\n", shdr->sh_offset);
    }

  elf_end (elf);
  exit (0);
}
