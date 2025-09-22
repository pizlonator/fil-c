/* Test program for reading and writing out the same file in-place
   Copyright (C) 2019 Red Hat, Inc.
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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include ELFUTILS_HEADER(elf)
#include <gelf.h>


int
main (int argc, const char *argv[])
{
  /* Takes the given file, and create a new identical one.  */
  if (argc != 2)
    {
      fprintf (stderr, "elfrdwrnop elf-file\n");
      exit (1);
    }

  elf_version (EV_CURRENT);

  const char *name = argv[1];
  printf ("elfrdwrdnop %s\n", name);

  int fd = open (name, O_RDWR);
  if (fd < 0)
    {
      fprintf (stderr, "Couldn't open file '%s': %s\n",
	       name, strerror (errno));
      exit (1);
    }

  Elf *elf = elf_begin (fd, ELF_C_RDWR, NULL);
  if (elf == NULL)
    {
      fprintf (stderr, "Couldn't open ELF file '%s': %s\n",
	       name, elf_errmsg (-1));
      exit (1);
    }

  /* Write everything to disk.  If there are any phdrs, then we want
     the exact same layout.  */
  size_t phnum;
  if (elf_getphdrnum (elf, &phnum) != 0)
    {
      printf ("cannot get phdrs: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (phnum > 0)
    elf_flagelf (elf, ELF_C_SET, ELF_F_LAYOUT);

  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (elf_end (elf) != 0)
    {
      printf ("couldn't cleanup elf '%s': %s\n", name, elf_errmsg (-1));
      exit (1);
    }

  if (close (fd) != 0)
    {
      printf ("couldn't close '%s': %s\n", name, strerror (errno));
      exit (1);
    }

  return 0;
}
