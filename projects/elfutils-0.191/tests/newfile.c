/* Copyright (C) 1999, 2000, 2001, 2002, 2005 Red Hat, Inc.
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

#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
print_ehdr (Elf32_Ehdr *ehdr)
{
  int n;

  for (n = 0; n < EI_NIDENT; ++n)
    printf (" %02x", ehdr->e_ident[n]);

  printf ("\ntype = %d\nmachine = %d\nversion = %d\nentry = %d\n"
          "phoff = %d\nshoff = %d\nflags = %d\nehsize = %d\n"
          "phentsize = %d\nphnum = %d\nshentsize = %d\nshnum = %d\n"
          "shstrndx = %d\n",
          ehdr->e_type,
          ehdr->e_machine,
          ehdr->e_version,
          ehdr->e_entry,
          ehdr->e_phoff,
          ehdr->e_shoff,
          ehdr->e_flags,
          ehdr->e_ehsize,
          ehdr->e_phentsize,
          ehdr->e_phnum,
          ehdr->e_shentsize,
          ehdr->e_shnum,
          ehdr->e_shstrndx);
}

int
main (int argc, char *argv[] __attribute__ ((unused)))
{
  Elf *elf;
  int result = 0;
  int fd;
  char fname[] = "newfile-XXXXXX";

  fd = mkstemp (fname);
  if (fd == -1)
    {
      printf ("cannot create temporary file: %m\n");
      exit (1);
    }
  /* Remove the file when we exit.  */
  unlink (fname);

  elf_version (EV_CURRENT);
  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  if (elf == NULL)
    {
      printf ("elf_begin: %s\n", elf_errmsg (-1));
      result = 1;
    }
  else
    {
      if (elf32_newehdr (elf) == NULL)
	{
	  printf ("elf32_newehdr: %s\n", elf_errmsg (-1));
	  result = 1;
	}
      else
        {
	  Elf32_Ehdr *ehdr = elf32_getehdr (elf);

	  if (ehdr == NULL)
	    {
	      printf ("elf32_getehdr: %s\n", elf_errmsg (-1));
	      result = 1;
	    }
	  else
	    {
	      int i;

	      if (argc > 1)
		/* Use argc as a debugging flag.  */
		print_ehdr (ehdr);

	      /* Some tests.  */
	      for (i = 0; i < EI_NIDENT; ++i)
		if (ehdr->e_ident[i] != 0)
		  {
		    printf ("ehdr->e_ident[%d] != 0\n", i);
		    result = 1;
		    break;
		  }

#define VALUE_TEST(name, val) \
	      if (ehdr->name != val)					      \
	        {							      \
		  printf ("ehdr->%s != %d\n", #name, val);		      \
		  result = 1;						      \
		}
#define ZERO_TEST(name) VALUE_TEST (name, 0)
	      ZERO_TEST (e_type);
	      ZERO_TEST (e_machine);
	      ZERO_TEST (e_version);
	      ZERO_TEST (e_entry);
	      ZERO_TEST (e_phoff);
	      ZERO_TEST (e_shoff);
	      ZERO_TEST (e_flags);
	      ZERO_TEST (e_ehsize);
	      ZERO_TEST (e_phentsize);
	      ZERO_TEST (e_phnum);
	      ZERO_TEST (e_shentsize);
	      ZERO_TEST (e_shnum);
	      ZERO_TEST (e_shstrndx);

	      if (elf32_newphdr (elf, 10) == NULL)
		{
		  printf ("elf32_newphdr: %s\n", elf_errmsg (-1));
		  result = 1;
		}
	      else
		{
		  if (argc > 1)
		    print_ehdr (ehdr);

		  ehdr = elf32_getehdr (elf);
		  if (ehdr == NULL)
		    {
		      printf ("elf32_getehdr (#2): %s\n", elf_errmsg (-1));
		      result = 1;
		    }
		  else
		    {
		      ZERO_TEST (e_type);
		      ZERO_TEST (e_machine);
		      ZERO_TEST (e_version);
		      ZERO_TEST (e_entry);
		      ZERO_TEST (e_phoff);
		      ZERO_TEST (e_shoff);
		      ZERO_TEST (e_flags);
		      ZERO_TEST (e_ehsize);
		      VALUE_TEST (e_phentsize, (int) sizeof (Elf32_Phdr));
		      VALUE_TEST (e_phnum, 10);
		      ZERO_TEST (e_shentsize);
		      ZERO_TEST (e_shnum);
		      ZERO_TEST (e_shstrndx);
		    }
		}
	    }
        }

      (void) elf_end (elf);
    }

  return result;
}
