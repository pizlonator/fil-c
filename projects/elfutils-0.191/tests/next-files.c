/* A variant of get-files test that uses dwarf_next_lines.
   Copyright (C) 2002, 2004, 2005, 2007, 2014, 2018 Red Hat, Inc.
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
#include <inttypes.h>
#include <libelf.h>
#include ELFUTILS_HEADER(dw)
#include <stdio.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);

      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if (dbg == NULL)
	{
	  printf ("%s not usable\n", argv[cnt]);
	  result = 1;
	  if (fd != -1)
	    close (fd);
	  continue;
	}

      Dwarf_Off off;
      Dwarf_Off next_off = 0;
      Dwarf_CU *cu = NULL;
      Dwarf_Files *files;
      size_t nfiles;
      int res;
      while ((res = dwarf_next_lines (dbg, off = next_off, &next_off, &cu,
				      &files, &nfiles, NULL, NULL)) == 0)
	{
	  printf ("off = %" PRIu64 "\n", off);

	  const char *const *dirs;
	  size_t ndirs;
	  if (dwarf_getsrcdirs (files, &dirs, &ndirs) != 0)
	    {
	      printf ("%s: cannot get include directories\n", argv[cnt]);
	      result = 1;
	      break;
	    }

	  if (dirs[0] == NULL)
	    puts (" dirs[0] = (null)");
	  else
	    printf (" dirs[0] = \"%s\"\n", dirs[0]);
	  for (size_t i = 1; i < ndirs; ++i)
	    printf (" dirs[%zu] = \"%s\"\n", i, dirs[i]);

	  for (size_t i = 0; i < nfiles; ++i)
	    printf (" file[%zu] = \"%s\"\n", i,
		    dwarf_filesrc (files, i, NULL, NULL));
	}

      if (res < 0)
	{
	  printf ("dwarf_next_lines failed: %s\n", dwarf_errmsg (-1));
	  result = 1;
	}

      dwarf_end (dbg);
      close (fd);
    }

  return result;
}
