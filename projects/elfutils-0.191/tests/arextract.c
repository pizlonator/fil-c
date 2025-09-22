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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <system.h>


int
main (int argc, char *argv[])
{
  int fd;
  Elf *elf;
  Elf *subelf;
  Elf_Cmd cmd;
  off_t offset;
  size_t todo;

  if (argc < 4)
    exit (1);

  /* Open the archive.  */
  fd = open (argv[1], O_RDONLY);
  if (fd == -1)
    {
      printf ("Cannot open input file: %m");
      exit (1);
    }

  /* Set the ELF version.  */
  elf_version (EV_CURRENT);

  /* Create an ELF descriptor.  */
  cmd = ELF_C_READ;
  elf = elf_begin (fd, cmd, NULL);
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

  /* Get the elements of the archive one after the other.  */
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      /* The the header for this element.  */
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);

      if (arhdr == NULL)
	{
	  printf ("cannot get arhdr: %s\n", elf_errmsg (-1));
	  exit (1);
	}

      if (strcmp (arhdr->ar_name, argv[2]) == 0)
	{
	  int outfd;

	  /* Get the offset of the file in the archive.  */
	  offset = elf_getbase (subelf);
	  if (offset == -1)
	    {
	      printf ("\
Failed to get base address for the archive element: %s\n",
		      elf_errmsg (-1));
	      exit (1);
	    }

	  /* Open the output file.  */
	  outfd = open (argv[3], O_CREAT | O_TRUNC | O_RDWR, DEFFILEMODE);
	  if (outfd == -1)
	    {
	      printf ("cannot open output file: %m");
	      exit (1);
	    }

	  /* Now write out the data.  */
	  todo = arhdr->ar_size;
	  while (todo > 0)
	    {
	      char buf[1024];
	      ssize_t n = pread (fd, buf, MIN (sizeof buf, todo), offset);
	      if (n == 0)
		break;

	      if (write (outfd, buf, n) != n)
		{
		  puts ("Writing output failed");
		  exit (1);
		}

	      offset += n;
	      todo -= n;
	    }

	  /* Check whether all the date was read and written out.  */
	  if (todo != 0)
	    {
	      puts ("Reading archive member failed.");
	      exit (1);
	    }

	  /* Close the descriptors.  */
	  if (elf_end (subelf) != 0 || elf_end (elf) != 0)
	    {
	      printf ("Freeing ELF descriptors failed: %s", elf_errmsg (-1));
	      exit (1);
	    }

	  close (outfd);
	  close (fd);

	  /* All went well.  */
	  exit (0);
	}

      /* Get next archive element.  */
      cmd = elf_next (subelf);
      if (elf_end (subelf) != 0)
	{
	  printf ("error while freeing sub-ELF descriptor: %s\n",
		  elf_errmsg (-1));
	  exit (1);
	}
    }

  /* When we reach this point we haven't found the given file in the
     archive.  */
  printf ("File `%s' not found in archive\n", argv[2]);
  exit (1);
}
