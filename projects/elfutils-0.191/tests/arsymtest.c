/* Copyright (C) 1999, 2000, 2002 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1999.

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
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int
main (int argc, char *argv[])
{
  int fd;
  FILE *fp;
  Elf *elf;
  Elf_Arsym *arsym;
  size_t narsym;

  if (argc < 3)
    exit (1);

  /* Open the archive.  */
  fd = open (argv[1], O_RDONLY);
  if (fd == -1)
    {
      printf ("Cannot open input file: %m");
      exit (1);
    }

  /* Open the output file.  */
  fp = fopen (argv[2], "w");
  if (fp == NULL)
    {
      printf ("Cannot open output file: %m");
      exit (1);
    }

  /* Set the ELF version.  */
  elf_version (EV_CURRENT);

  /* Create an ELF descriptor.  */
  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("Cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* If it is no archive punt.  */
  if (elf_kind (elf) != ELF_K_AR)
    {
      printf ("`%s' is no archive\n", argv[1]);
      exit (1);
    }

  /* Now get the index of the archive.  */
  arsym = elf_getarsym (elf, &narsym);
  if (arsym == NULL)
    {
      printf ("Cannot get archive index: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* If there is no element in the index do nothing.  There always is
     an empty entry at the end which is included in the count and
     which we want to skip.  */
  if (narsym-- > 1)
    while (narsym-- > 0)
      {
	Elf *subelf;
	Elf_Arhdr *arhdr;

	if (elf_rand (elf, arsym[narsym].as_off) != arsym[narsym].as_off)
	  {
	    printf ("random access for symbol `%s' fails: %s\n",
		    arsym[narsym].as_name, elf_errmsg (-1));
	    exit (1);
	  }

	subelf = elf_begin (fd, ELF_C_READ, elf);
	if (subelf == NULL)
	  {
	    printf ("Cannot create ELF descriptor for archive member: %s\n",
		    elf_errmsg (-1));
	    exit (1);
	  }

	arhdr = elf_getarhdr (subelf);
	if (arhdr == NULL)
	  {
	    printf ("Cannot get archive header for element `%s': %s\n",
		    arsym[narsym].as_name, elf_errmsg (-1));
	    exit (1);
	  }

	/* Now print what we actually want.  */
	fprintf (fp, "%s in %s\n", arsym[narsym].as_name, arhdr->ar_name);

	/* Free the ELF descriptor.  */
	if (elf_end (subelf) != 0)
	  {
	    printf ("Error while freeing subELF descriptor: %s\n",
		    elf_errmsg (-1));
	    exit (1);
	  }
      }

  /* Free the ELF descriptor.  */
  if (elf_end (elf) != 0)
    {
      printf ("Error while freeing ELF descriptor: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);
  fclose (fp);

  return 0;
}
