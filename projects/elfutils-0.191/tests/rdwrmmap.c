/* Copyright (C) 2006 Red Hat, Inc.
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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>
#include "system.h"

int
main (int argc __attribute__ ((unused)), char *argv[])
{
  int fd = open (argv[1], O_RDWR);
  if (fd < 0)
    error (2, errno, "open: %s", argv[1]);

  if (elf_version (EV_CURRENT) == EV_NONE)
    error (1, 0, "libelf version mismatch");

  Elf *elf = elf_begin (fd, ELF_C_RDWR_MMAP, NULL);
  if (elf == NULL)
    error (1, 0, "elf_begin: %s", elf_errmsg (-1));

  if (elf_update (elf, ELF_C_WRITE) < 0)
    error (1, 0, "elf_update: %s", elf_errmsg (-1));

  elf_end (elf);
  close (fd);

  return 0;
}
