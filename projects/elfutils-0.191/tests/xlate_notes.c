/* Test program for extracting ELF Note headers and getting whole notes.
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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include ELFUTILS_HEADER(elf)
#include <gelf.h>

int
main (int argc, char *argv[])
{
  if (argc != 2)
    {
      printf ("No ELF file given as argument\n");
      exit (1);
    }

  const char *fname = argv[1];

  // Initialize libelf.
  elf_version (EV_CURRENT);

  /* Read the ELF from disk now.  */
  int fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open '%s': %s\n", fname, strerror (errno));
      exit (1);
    }

  Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      exit (1);
    }

  GElf_Ehdr ehdr;
  if (gelf_getehdr (elf, &ehdr) == NULL)
    {
      printf ("cannot get Ehdr: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Search for all SHT_NOTE sections.  */
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      /* Get the header.  */
      GElf_Shdr shdr;
      if (gelf_getshdr (scn, &shdr) == NULL)
	{
	  printf ("couldn't get shdr: %s\n", elf_errmsg (-1));
	  exit (1);
	}

      if (shdr.sh_type == SHT_NOTE)
	{
	  printf ("Notes in section %zd:\n", elf_ndxscn (scn));

	  Elf_Data *raw = elf_rawdata (scn, NULL);
	  if (raw == NULL)
	    {
	      printf ("couldn't get raw data: %s\n", elf_errmsg (-1));
	      exit (1);
	    }

	  Elf_Data *data = elf_getdata (scn, NULL);
	  if (data == NULL)
	    {
	      printf ("couldn't get data: %s\n", elf_errmsg (-1));
	      exit (1);
	    }

	  size_t off = 0;
	  size_t next;
	  GElf_Nhdr nhdr;
	  size_t n_off;
	  size_t d_off;
	  while ((next = gelf_getnote (data, off, &nhdr, &n_off, &d_off)) > 0)
	    {
	      /* Now just get the note header "raw" (don't
		 copy/translate the note data). This only handles
		 traditional GNU ELF Notes, so we still use the next
		 from gelf_getnote (padding is different for new style
		 ELF_T_NHDR8 notes).  */
	      Elf32_Nhdr nh;
	      Elf_Data src =
                {
                  .d_version = EV_CURRENT, .d_type = ELF_T_NHDR,
		  .d_size = sizeof nh
                };
	      Elf_Data dst = src;
	      src.d_buf = raw->d_buf + off;
	      dst.d_buf = &nh;

	      if (elf32_xlatetom (&dst, &src, ehdr.e_ident[EI_DATA]) == NULL)
		{
		  printf ("couldn't xlate note: %s\n", elf_errmsg (-1));
		  exit (1);
		}

	      printf ("type: %" PRId32 ",%" PRId32
		      ", namesz: %" PRId32 ",%" PRId32
		      ", descsz: %" PRId32 ",%" PRId32 "\n",
		      nhdr.n_type, nh.n_type,
		      nhdr.n_namesz, nh.n_namesz,
		      nhdr.n_descsz, nh.n_descsz);

	      if (nhdr.n_type != nh.n_type
		  || nhdr.n_namesz != nh.n_namesz
		  || nhdr.n_descsz != nh.n_descsz)
		{
		  printf ("Nhdrs not equal!\n");
		  exit (1);
		}

	      off = next;
	    }
	}

    }

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  return 0;
}
