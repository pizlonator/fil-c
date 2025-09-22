/* Copyright (C) 2015 Red Hat, Inc.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

#include ELFUTILS_HEADER(elf)
#include ELFUTILS_HEADER(dwelf)
#include <gelf.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  elf_version (EV_CURRENT);

  for (cnt = 1; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);

      Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
      if (elf == NULL)
	{
	  printf ("%s not usable %s\n", argv[cnt], elf_errmsg (-1));
	  result = 1;
	  close (fd);
	  continue;
	}

      size_t shdrstrndx;
      if (elf_getshdrstrndx (elf, &shdrstrndx) == -1)
	{
	  printf ("elf_getshdrstrnd failed %s\n", elf_errmsg (-1));
	  result = 1;
	  close (fd);
	  continue;
	}

      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  int idx = elf_ndxscn (scn);
	  GElf_Shdr shdr;
	  if (gelf_getshdr (scn, &shdr) == NULL)
	    {
	      printf ("gelf_getshdr failed: %s\n", elf_errmsg (-1));
	      result = 1;
	      break;
	    }

	  const char *sname = elf_strptr (elf, shdrstrndx, shdr.sh_name);
	  if (sname == NULL)
	    {
	      printf ("couldn't get section name: %s\n", elf_errmsg (-1));
	      result = 1;
	      break;
	    }

	  if (startswith (sname, ".zdebug"))
	    {
	      ssize_t size;
	      if ((size = dwelf_scn_gnu_compressed_size (scn)) == -1)
		{
		  printf ("dwelf_scn_gnu_compressed_size failed: %s\n",
			  elf_errmsg (-1));
		  result = 1;
		  break;
		}
	      printf ("section %d: GNU Compressed size: %zx\n", idx, size);
	    }
	}

      elf_end (elf);
      close (fd);
    }

  return result;
}
