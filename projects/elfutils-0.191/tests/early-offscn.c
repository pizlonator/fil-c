/* Copyright (C) 2008 Red Hat, Inc.
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
#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include "system.h"

int
main (int argc, char *argv[])
{
  if (argc < 2)
    error (1, 0, "Usage: %s FILE OFFSET", argv[0]);

  /* Set the ELF version.  */
  elf_version (EV_CURRENT);

  /* Open the archive.  */
  int fd = open (argv[1], O_RDONLY);
  if (fd < 0)
    error (1, errno, "cannot open '%s'", argv[1]);

  Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    error (2, 0, "elf_begin: %s", elf_errmsg (-1));

  Elf_Scn *scn = gelf_offscn (elf, strtoull (argv[2], NULL, 0));
  if (scn == NULL)
    error (3, 0, "gelf_offscn: %s", elf_errmsg (-1));

  elf_end (elf);
  return 0;
}
