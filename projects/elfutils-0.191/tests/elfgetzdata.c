/* Copyright (C) 2015 Red Hat, Inc.
   Copyright (C) 2024 Mark J. Wielaard <mark@klomp.org>
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

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libelf.h>
#include <errno.h>
#include <gelf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  if (argc < 3
      || (strcmp (argv[1], "read") != 0
          && strcmp (argv[1], "mmap") != 0
          && strcmp (argv[1], "mem") != 0))
    {
      printf ("Usage: (read|mmap|mem) files...\n");
      return -1;
    }

  bool do_read = strcmp (argv[1], "read") == 0;
  bool do_mmap = strcmp (argv[1], "mmap") == 0;
  bool do_mem = strcmp (argv[1], "mem") == 0;

  elf_version (EV_CURRENT);

  for (cnt = 2; cnt < argc; ++cnt)
    {
      int fd = open (argv[cnt], O_RDONLY);
      void *map_address = NULL;
      size_t map_size = 0;

      Elf *elf;
      if (do_read)
	elf = elf_begin (fd, ELF_C_READ, NULL);
      else if (do_mmap)
	elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
      else
        {
	  assert (do_mem);
	  // We mmap the memory ourselves, explicitly PROT_READ only
	  struct stat st;
	  if (fstat (fd, &st) != 0)
	    {
	      printf ("%s cannot stat %s\n", argv[cnt], strerror (errno));
	      result = 1;
	      close (fd);
	      continue;
	    }
	  map_size = st.st_size;
	  map_address = mmap (NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
	  if (map_address == MAP_FAILED)
	    {
	      printf ("%s cannot mmap %s\n", argv[cnt], strerror (errno));
	      result = 1;
	      close (fd);
	      continue;
	    }
	  if (map_size < EI_NIDENT
	      || memcmp (map_address, ELFMAG, SELFMAG) != 0)
	    {
	      printf ("%s isn't an ELF file\n", argv[cnt]);
	      result = 1;
	      munmap (map_address, map_size);
	      close (fd);
	      continue;
	    }
	  elf = elf_memory (map_address, map_size);
        }
      if (elf == NULL)
	{
	  printf ("%s not usable %s\n", argv[cnt], elf_errmsg (-1));
	  result = 1;
	  close (fd);
	  continue;
	}

      /* To get the section names.  */
      size_t strndx;
      elf_getshdrstrndx (elf, &strndx);

      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  size_t idx = elf_ndxscn (scn);
	  GElf_Shdr mem;
	  GElf_Shdr *shdr = gelf_getshdr (scn, &mem);
	  const char *name = elf_strptr (elf, strndx, shdr->sh_name);
	  if ((shdr->sh_flags & SHF_COMPRESSED) != 0)
	    {
	      /* Real compressed section.  */
	      if (elf_compress (scn, 0, 0) < 0)
		{
		  printf ("elf_compress failed for section %zd: %s\n",
			  idx, elf_errmsg (-1));
		  return -1;
		}
	      Elf_Data *d = elf_getdata (scn, NULL);
	      printf ("%zd: %s, ELF compressed, size: %zx\n",
		      idx, name, d->d_size);
	    }
	  else
	    {
	      /* Maybe an old GNU compressed .z section?  */
	      if (name[0] == '.' && name[1] == 'z')
		{
		  if (elf_compress_gnu (scn, 0, 0) < 0)
		    {
		      printf ("elf_compress_gnu failed for section %zd: %s\n",
			      idx, elf_errmsg (-1));
		      return -1;
		    }
		  Elf_Data *d = elf_getdata (scn, NULL);
		  printf ("%zd: %s, GNU compressed, size: %zx\n",
			  idx, name, d->d_size);
		}
	      else
		printf ("%zd: %s, NOT compressed\n", idx, name);
	    }
	}

      elf_end (elf);
      close (fd);

      if (do_mem)
        {
          /* Make sure we can still get at the memory.  */
	  if (memcmp (map_address, ELFMAG, SELFMAG) != 0)
	    {
	      printf ("%s isn't an ELF file anymore???\n", argv[cnt]);
	      result = 1;
	    }
	  if (munmap (map_address, map_size) != 0)
	    {
	      printf ("%s cannot munmap %s\n", argv[cnt], strerror (errno));
	      result = 1;
	    }
        }
    }

  return result;
}
