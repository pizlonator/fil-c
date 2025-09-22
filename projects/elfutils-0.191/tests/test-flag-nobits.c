/* Copyright (C) 2010 Red Hat, Inc.
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

#include <fcntl.h>
#include <stdlib.h>
#include <gelf.h>

int
main (int argc, char **argv)
{
  if (argc != 2)
    abort ();

  elf_version (EV_CURRENT);

  int fd = open (argv[1], O_RDONLY);
  Elf *stripped = elf_begin (fd, ELF_C_READ, NULL);

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (stripped, scn)) != NULL)
    elf_flagdata (elf_getdata (scn, NULL), ELF_C_SET, ELF_F_DIRTY);

  elf_end (stripped);
  return 0;
}
