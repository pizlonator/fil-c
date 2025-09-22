/* Test program for dwarf_cu_dwp_section_info
   Copyright (c) 2023 Meta Platforms, Inc. and affiliates.
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
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <dwarf.h>
#include ELFUTILS_HEADER(dw)

int
main (int argc, char *argv[])
{
  for (int i = 1; i < argc; i++)
    {
      printf ("file: %s\n", argv[i]);
      int fd = open (argv[i], O_RDONLY);
      Dwarf *dbg = dwarf_begin (fd, DWARF_C_READ);
      if (dbg == NULL)
	{
	  printf ("%s not usable: %s\n", argv[i], dwarf_errmsg (-1));
	  return -1;
	}

      Dwarf_CU *cu = NULL;
      while (dwarf_get_units (dbg, cu, &cu, NULL, NULL, NULL, NULL) == 0)
	{
#define SECTION_INFO(section) do {					\
	    printf (#section ": ");					\
	    Dwarf_Off offset, size;					\
	    if (dwarf_cu_dwp_section_info (cu, DW_SECT_##section,	\
					   &offset, &size) == 0)	\
	      printf ("0x%" PRIx64 " 0x%" PRIx64 "\n", offset, size);	\
	    else							\
	      printf ("%s\n", dwarf_errmsg (-1));			\
	  } while (0)
	  SECTION_INFO (INFO);
	  SECTION_INFO (TYPES);
	  SECTION_INFO (ABBREV);
	  SECTION_INFO (LINE);
	  SECTION_INFO (LOCLISTS);
	  SECTION_INFO (STR_OFFSETS);
	  SECTION_INFO (MACRO);
	  SECTION_INFO (RNGLISTS);
	  printf ("\n");
	}

      dwarf_end (dbg);
      close (fd);
    }

  return 0;
}
