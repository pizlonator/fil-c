/* Test program for dwelf_elf_e_machine_string
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

#include <system.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include ELFUTILS_HEADER(dwelf)

int
main (int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++)
    {
      long val;
      int em;
      const char *machine;

      errno = 0;
      if (startswith (argv[i], "0x"))
	val = strtol (&argv[i][2], NULL, 16);
      else
	val = strtol (argv[i], NULL, 10);

      if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
           || (errno != 0 && val == 0))
	{
          perror ("strtol");
          exit (EXIT_FAILURE);
	}

      em = val;
      assert (em == val);

      machine = dwelf_elf_e_machine_string (em);
      printf ("0x%x %s\n", em, machine);
      assert (machine != NULL);
    }

  return 0;
}
