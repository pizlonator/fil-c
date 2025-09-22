/* Copyright (C) 1999, 2000, 2001, 2002, 2005 Red Hat, Inc.
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

#include <assert.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int
main (void)
{
  Elf *elf;
  int fd;
  Elf_Scn *section;

  if (elf_version (EV_CURRENT) == EV_NONE)
    {
      fprintf (stderr, "library fd of date\n");
      exit (1);
    }

  char name[] = "test.XXXXXX";
  fd = mkstemp (name);
  if (fd < 0)
    {
      fprintf (stderr, "Failed to open fdput file: %s\n", name);
      exit (1);
    }
  unlink (name);

  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  if (elf == NULL)
    {
      fprintf (stderr, "Failed to elf_begin fdput file: %s\n", name);
      exit (1);
    }

  section = elf_newscn (elf);
  section = elf_nextscn (elf, section);
  assert (section == NULL);

  elf_end (elf);
  close (fd);

  return 0;
}
