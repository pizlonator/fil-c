/* Copyright (C) 1998, 1999, 2000, 2001, 2002, 2004, 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <fcntl.h>
#include <inttypes.h>
#include ELFUTILS_HEADER(dw)
#include <stdio.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int cnt;

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);
      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if  (dbg == NULL)
	{
	  printf ("%s not usable: %s\n", argv[cnt], dwarf_errmsg (-1));
	  close  (fd);
	  continue;
	}

      Dwarf_Off cuoff = 0;
      Dwarf_Off old_cuoff = 0;
      size_t hsize;
      while (dwarf_nextcu (dbg, cuoff, &cuoff, &hsize, NULL, NULL, NULL) == 0)
	{
	  /* Get the DIE for the CU.  */
	  Dwarf_Die die;
 	  if (dwarf_offdie (dbg, old_cuoff + hsize, &die) == NULL)
	    /* Something went wrong.  */
	    break;

	  /* Test something obviously wrong.  */
	  Dwarf_Abbrev *a = dwarf_getabbrev (&die, (Dwarf_Off) -1, NULL);
	  if (a != NULL)
	    {
	      printf ("dwarf_getabbrev -1 succeeded?\n");
	      break;
	    }

	  Dwarf_Off offset = 0;

	  while (1)
	    {
	      size_t length;
	      Dwarf_Abbrev *abbrev = dwarf_getabbrev (&die, offset, &length);
	      if (abbrev == NULL || abbrev == DWARF_END_ABBREV)
		/* End of the list.  */
		break;

	      unsigned tag = dwarf_getabbrevtag (abbrev);
	      if (tag == 0)
		{
		  printf ("dwarf_getabbrevtag at offset %llu returned error: %s\n",
			  (unsigned long long int) offset,
			  dwarf_errmsg (-1));
		  break;
		}

	      unsigned code = dwarf_getabbrevcode (abbrev);
	      if (code == 0)
		{
		  printf ("dwarf_getabbrevcode at offset %llu returned error: %s\n",
			  (unsigned long long int) offset,
			  dwarf_errmsg (-1));
		  break;
		}

	      int children = dwarf_abbrevhaschildren (abbrev);
	      if (children < 0)
		{
		  printf ("dwarf_abbrevhaschildren at offset %llu returned error: %s\n",
			  (unsigned long long int) offset,
			  dwarf_errmsg (-1));
		  break;
		}

	      printf ("abbrev[%llu]: code = %u, tag = %u, children = %d\n",
		      (unsigned long long int) offset, code, tag, children);

	      size_t attrcnt;
	      if (dwarf_getattrcnt (abbrev, &attrcnt) != 0)
		{
		  printf ("dwarf_getattrcnt at offset %llu returned error: %s\n",
			  (unsigned long long int) offset,
			  dwarf_errmsg (-1));
		  break;
		}

	      unsigned int attr_num;
	      unsigned int attr_form;
	      Dwarf_Off aboffset;
	      size_t j;
	      for (j = 0; j < attrcnt; ++j)
		if (dwarf_getabbrevattr (abbrev, j, &attr_num, &attr_form,
					 &aboffset))
		  printf ("dwarf_getabbrevattr for abbrev[%llu] and index %zu failed\n",
			  (unsigned long long int) offset, j);
		else
		  printf ("abbrev[%llu]: attr[%zu]: code = %u, form = %u, offset = %" PRIu64 "\n",
			  (unsigned long long int) offset, j, attr_num,
			  attr_form, (uint64_t) aboffset);

	      offset += length;
	    }

	  old_cuoff = cuoff;
	}

      if (dwarf_end (dbg) != 0)
	printf ("dwarf_end failed for %s: %s\n", argv[cnt],
		dwarf_errmsg (-1));

      close (fd);
    }

  return 0;
}
