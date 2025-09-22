/* Copyright (C) 2007 Red Hat, Inc.
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

#include <ar.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <unistd.h>


static int handle (const char *fname);


int
main (int argc, char *argv[])
{
  elf_version (EV_CURRENT);

  int result = 0;
  if (argc == 1)
    result = handle ("a.out");
  else
    for (int i = 1; i < argc; ++i)
      result |= handle (argv[1]);

  return result;
}


static int
handle (const char *fname)
{
  int fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open '%s': %m\n", fname);
      return 1;
    }

  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (elf == NULL)
    {
      printf ("cannot get ELF handling for '%s': %s\n",
	      fname, elf_errmsg (-1));
      close (fd);
      return 1;
    }

  if (elf_kind (elf) != ELF_K_AR)
    {
      printf ("'%s' is no archive\n", fname);
      elf_end (elf);
      close (fd);
      return 1;
    }

  printf ("%s:\n", fname);
  Elf *subelf = NULL;
  Elf_Cmd cmd = ELF_C_READ_MMAP;
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);
      if (arhdr == NULL)
	{
	  printf ("cannot get archive header in '%s': %s\n",
		  fname, elf_errmsg (-1));
	  elf_end (subelf);
	  elf_end (elf);
	  close (fd);
	  return 1;
	}

      off_t off = elf_getaroff (subelf);

      printf ("\nOffset    %llu\n"
	      "  Name    %s\n"
	      "  Date    %ld\n"
	      "  UID     %d\n"
	      "  GID     %d\n"
	      "  Mode    %o\n"
	      "  Size    %lld\n",
	      (unsigned long long int) off,
	      arhdr->ar_name, (long int) arhdr->ar_date, (int) arhdr->ar_uid,
	      (int) arhdr->ar_gid,
	      (int) arhdr->ar_mode, (long long int) arhdr->ar_size);

      cmd = elf_next (subelf);
      elf_end (subelf);
    }

  close (fd);

  return 0;
}
