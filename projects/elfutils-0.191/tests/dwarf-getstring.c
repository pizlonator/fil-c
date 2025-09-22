/* Copyright (C) 2011 Red Hat, Inc.
   This file is part of elfutils.
   Written by Marek Polacek <mpolacek@redhat.com>, 2011.

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

#include ELFUTILS_HEADER(dwfl)
#include <assert.h>
#include <dwarf.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      Dwarf_Off offset = 0;
      size_t len;

      int fd = open (argv[cnt], O_RDONLY);
      if (fd == -1)
	{
	  printf ("cannot open '%s': %m\n", argv[cnt]);
	  return 1;
	}

      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if (dbg == NULL)
	{
	  printf ("%s not usable: %s\n", argv[cnt], dwarf_errmsg (-1));
	  close (fd);
	  return 1;
	}

      /* Try to use NULL Dwarf object.  */
      const char *str = dwarf_getstring (NULL, offset, &len);
      assert (str == NULL);

      /* Use insane offset.  */
      str = dwarf_getstring (dbg, ~0UL, &len);
      assert (str == NULL);

      /* Now do some real work.  */
      for (int i = 0; i < 100; ++i)
	{
	  str = dwarf_getstring (dbg, offset, &len);
	  puts (str);

	  /* Advance.  */
	  offset += len + 1;
	}

      dwarf_end (dbg);
      close (fd);
    }

  return 0;
}
