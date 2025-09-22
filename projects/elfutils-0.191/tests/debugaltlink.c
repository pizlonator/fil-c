/* Test program for dwelf_dwarf_gnu_debugaltlink, print name and build ID.
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
#include <err.h>
#include <errno.h>
#include ELFUTILS_HEADER(dw)
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

      Dwarf *dwarf = dwarf_begin (fd, DWARF_C_READ);
      if (dwarf == NULL)
	{
	  printf("%s: dwarf_begin failed: %s\n", file, dwarf_errmsg (-1));
	  close (fd);
	  continue;
	}

      const char *name;
      const void *build_id;
      ssize_t ret = dwelf_dwarf_gnu_debugaltlink
	(dwarf, &name, &build_id);
      switch (ret)
	{
	case 0:
	  printf ("%s: <no .gnu_debugaltlink section>\n", file);
	  break;
	case -1:
	  errx (1, "dwelf_dwarf_gnu_debugaltlink (%s): %s",
		file, dwarf_errmsg (-1));
	default:
	  printf ("%s: %s, build ID: ", file, name);
	  const unsigned char *p = build_id;
	  const unsigned char *end = p + ret;
	  while (p < end)
	      printf("%02x", (unsigned)*p++);
	  putchar('\n');
	}

      dwarf_end (dwarf);
      close (fd);
    }

  return 0;
}
