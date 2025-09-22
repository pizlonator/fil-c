/* Test program for elf_strptr function.
   Copyright (C) 2015 Red Hat, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include ELFUTILS_HEADER(elf)
#include <gelf.h>

int
main (int argc, char *argv[])
{
  if (argc != 2)
    {
      printf ("No ELF file given as argument");
      exit (1);
    }

  const char *fname = argv[1];

  // Initialize libelf.
  elf_version (EV_CURRENT);

  /* Read the ELF from disk now.  */
  int fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open `%s' read-only: %s\n", fname, strerror (errno));
      exit (1);
    }

  Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor read-only: %s\n", elf_errmsg (-1));
      exit (1);
    }

  size_t ndx;
  if (elf_getshdrstrndx (elf, &ndx) != 0)
    {
      printf ("cannot get section header table index: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (ndx == SHN_UNDEF)
    {
      printf ("ELF file `%s' doesn't have a section header table index", fname);
      exit (1);
    }

  printf ("Strings in section %zd:\n", ndx);

  size_t off = 0;
  const char *str = elf_strptr (elf, ndx, off);
  while (str != NULL)
    {
      printf ("[%zx] '%s'\n", off, str);
      off += strlen (str) + 1;
      str = elf_strptr (elf, ndx, off);
    }

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  return 0;
}
