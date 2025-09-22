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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libelf.h>
#include <gelf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int result = 0;
  int cnt;

  if (argc < 3
      || (strcmp (argv[1], "elf") != 0
	  && strcmp (argv[1], "gnu") != 0))
    {
      printf ("Usage: (elf|gnu) files...\n");
      return -1;
    }

  int gnu;
  if (strcmp (argv[1], "gnu") == 0)
    gnu = 1;
  else
    gnu = 0;

  elf_version (EV_CURRENT);

  for (cnt = 2; cnt < argc; ++cnt)
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
	  if (shdr->sh_type == SHT_NOBITS
	      || (shdr->sh_flags & SHF_ALLOC) != 0)
	    {
	      printf ("Cannot compress %zd %s\n", idx, name);
	    }
	  else if ((shdr->sh_flags & SHF_COMPRESSED) != 0
		   || startswith (name, ".zdebug"))
	    {
	      printf ("Already compressed %zd %s\n", idx, name);
	    }
	  else
	    {
	      size_t orig_size = shdr->sh_size;
	      printf ("Lets compress %zd %s, size: %" PRId64 "\n",
		      idx, name, shdr->sh_size);
	      Elf_Data *d = elf_getdata (scn, NULL);
	      if (d == NULL)
		{
		  printf ("Couldn't get orig data for section %zd\n", idx);
		  return -1;
		}
	      /* Make a copy so we can compare after
		 compression/decompression.  */
	      if (d->d_size != orig_size)
		{
		  printf ("Unexpected data size for orig section %zd\n", idx);
		  return -1;
		}
	      char *orig_buf = NULL;
	      if (orig_size > 0)
		{
		  orig_buf = malloc (d->d_size);
		  if (orig_buf == NULL)
		    {
		      printf ("No memory to copy section %zd data\n", idx);
		      return -1;
		    }
		  memcpy (orig_buf, d->d_buf, orig_size);
		}

	      bool forced = false;
	      if (gnu)
		{
		  int res = elf_compress_gnu (scn, 1, 0);
		  if (res == 0)
		    {
		      forced = true;
		      res = elf_compress_gnu (scn, 1, ELF_CHF_FORCE);
		    }
		  if (res < 0)
		    {
		      printf ("elf_compress_gnu%sfailed for section %zd: %s\n",
			      forced ? " (forced) " : " ",
			      idx, elf_errmsg (-1));
		      return -1;
		    }
		}
	      else
		{
		  int res = elf_compress (scn, ELFCOMPRESS_ZLIB, 0);
		  if (res == 0)
		    {
		      forced = true;
		      res = elf_compress (scn, ELFCOMPRESS_ZLIB, ELF_CHF_FORCE);
		    }
		  if (res < 0)
		    {
		      printf ("elf_compress%sfailed for section %zd: %s\n",
			      forced ? " (forced) " : " ",
			      idx, elf_errmsg (-1));
		      return -1;
		    }
		}
	      GElf_Shdr newmem;
	      GElf_Shdr *newshdr = gelf_getshdr (scn, &newmem);
	      size_t new_size = newshdr->sh_size;
	      d = elf_getdata (scn, NULL);
	      // Don't check this, might depend on zlib implementation.
	      // fprintf (stderr, "  new_size: %zd\n", new_size);
	      if (d->d_size != new_size)
		{
		  printf ("Unexpected data size for compressed section %zd\n",
			  idx);
		  return -1;
		}

	      if (forced && new_size < orig_size)
		{
		  printf ("section %zd forced to compress, but size smaller\n",
			  idx);
		  return -1;
		}

	      if (! forced && new_size >= orig_size)
		{
		  printf ("section %zd compressed to bigger size\n",
			  idx);
		  return -1;
		}

	      if (new_size == orig_size
		  && (orig_buf == NULL
		      || memcmp (orig_buf, d->d_buf, orig_size) == 0))
		{
		  printf ("section %zd didn't compress\n", idx);
		  return -1;
		}

	      if (gnu)
		{
		  if (elf_compress_gnu (scn, 0, 0) < 0)
		    {
		      printf ("elf_[un]compress_gnu failed for section %zd: %s\n",
			      idx, elf_errmsg (-1));
		      return -1;
		    }
		}
	      else
		{
		  if (elf_compress (scn, 0, 0) < 0)
		    {
		      printf ("elf_[un]compress failed for section %zd: %s\n",
			      idx, elf_errmsg (-1));
		      return -1;
		    }
		}
	      GElf_Shdr newermem;
	      GElf_Shdr *newershdr = gelf_getshdr (scn, &newermem);
	      size_t newer_size = newershdr->sh_size;
	      d = elf_getdata (scn, NULL);
	      // fprintf (stderr, "  newer_size: %zd\n", newer_size);
	      if (d->d_size != newer_size)
		{
		  printf ("Unexpected data size for compressed section %zd\n",
			  idx);
		  return -1;
		}
	      if (newer_size != orig_size
		  && (orig_buf == NULL
		      || memcmp (orig_buf, d->d_buf, orig_size) != 0))
		{
		  printf ("section %zd didn't correctly uncompress\n", idx);
		  return -1;
		}
	      free (orig_buf);
	      // Recompress the string table, just to make sure
	      // everything keeps working. See elf_strptr above.
	      if (! gnu && idx == strndx
		  && elf_compress (scn, ELFCOMPRESS_ZLIB, 0) < 0)
		{
		  printf ("couldn't recompress section header strings: %s\n",
			  elf_errmsg (-1));
		  return -1;
		}
	    }
	}

      elf_end (elf);
      close (fd);
    }

  return result;
}
