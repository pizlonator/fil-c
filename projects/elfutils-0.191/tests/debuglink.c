/* Test program for dwelf_elf_gnu_debuglink, print name and crc.
   Copyright (C) 2014 Red Hat, Inc.
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
#include <errno.h>
#include ELFUTILS_HEADER(dwelf)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "system.h"

int
main (int argc, char *argv[])
{
  if (argc < 2)
    error (EXIT_FAILURE, 0, "No input file given");

  elf_version (EV_CURRENT);

  for (int i = 1; i < argc; i++)
    {
      const char *file = argv[i];
      int fd = open (file, O_RDONLY);
      if (fd < 0)
	error (EXIT_FAILURE, errno, "couldn't open file '%s'", file);

      Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
      if (elf == NULL)
	error (EXIT_FAILURE, 0, "elf_begin failed for '%s': %s",
	       file, elf_errmsg (-1));

      GElf_Word crc;
      const char *debug = dwelf_elf_gnu_debuglink (elf, &crc);
      if (debug == NULL)
	printf ("%s: <no gnu_debuglink file>\n", file);
      else
	printf ("%s: %s, crc: %" PRIx32 "\n", file, debug, crc);

      elf_end (elf);
      close (fd);
    }

  return 0;
}
