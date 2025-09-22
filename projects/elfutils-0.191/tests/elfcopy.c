/* Test program for copying a whole ELF file using libelf.
   Copyright (C) 2018 Red Hat, Inc.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include ELFUTILS_HEADER(elf)
#include <gelf.h>


/* shstrndx is special, might overflow into section zero header sh_link.  */
static int
setshstrndx (Elf *elf, size_t ndx)
{
  printf ("setshstrndx: %zd\n", ndx);

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    return -1;

  if (ndx < SHN_LORESERVE)
    ehdr->e_shstrndx = ndx;
  else
    {
      ehdr->e_shstrndx = SHN_XINDEX;
      Elf_Scn *zscn = elf_getscn (elf, 0);
      GElf_Shdr zshdr_mem;
      GElf_Shdr *zshdr = gelf_getshdr (zscn, &zshdr_mem);
      if (zshdr == NULL)
	return -1;
      zshdr->sh_link = ndx;
      if (gelf_update_shdr (zscn, zshdr) == 0)
	return -1;
    }

  if (gelf_update_ehdr (elf, ehdr) == 0)
    return -1;

  return 0;
}

/* Copies all elements of an ELF file either using mmap or read.  */
static void
copy_elf (const char *in, const char *out, bool use_mmap, bool reverse_offs)
{
  printf ("\ncopy_elf: %s -> %s (%s,%s)\n", in, out,
	  use_mmap ? "mmap" : "read",
	  reverse_offs ? "reverse" : "same");

  /* Existing ELF file.  */
  int fda = open (in, O_RDONLY);
  if (fda < 0)
    {
      fprintf (stderr, "Couldn't open file '%s': %s\n",
	       in, strerror (errno));
      exit (1);
    }

  Elf *elfa = elf_begin (fda, use_mmap ? ELF_C_READ_MMAP : ELF_C_READ, NULL);
  if (elfa == NULL)
    {
      fprintf (stderr, "Couldn't open ELF file '%s': %s\n",
	       in, elf_errmsg (-1));
      exit (1);
    }

  /* Open new file.  */
  int fdb = open (out, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fdb < 0)
    {
      fprintf (stderr, "Couldn't create file '%s': %s\n",
	       out, strerror (errno));
      exit (1);
    }

  Elf *elfb = elf_begin (fdb, use_mmap ? ELF_C_WRITE_MMAP : ELF_C_WRITE, NULL);
  if (elfb == NULL)
    {
      fprintf (stderr, "Couldn't create ELF file '%s': %s\n",
	       out, elf_errmsg (-1));
      exit (1);
    }

  // Copy ELF header.
  GElf_Ehdr ehdr_mema;
  GElf_Ehdr *ehdra = gelf_getehdr (elfa, &ehdr_mema);
  if (ehdra == NULL)
    {
      printf ("cannot get ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  int class = gelf_getclass (elfa);
  // Create an ELF header.
  if (gelf_newehdr (elfb, class) == 0)
    {
      printf ("cannot create ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* New elf header is an exact copy.  */
  GElf_Ehdr ehdr_memb;
  GElf_Ehdr *ehdrb = &ehdr_memb;
  *ehdrb = *ehdra;
  if (gelf_update_ehdr (elfb, ehdrb) == 0)
    {
      printf ("cannot update ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* shstrndx is special.  (Technically phdrnum and shdrnum are also
     special, but they are handled by libelf.)  */
  size_t shstrndx;
  if (elf_getshdrstrndx (elfa, &shstrndx) < 0)
    {
      printf ("cannot get shstrndx: %s\n", elf_errmsg (-1));
      exit (1);
    }
  if (setshstrndx (elfb, shstrndx) < 0)
    {
      printf ("cannot set shstrndx: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* If there are phdrs, copy them over.  */
  size_t phnum;
  if (elf_getphdrnum (elfa, &phnum) != 0)
    {
      printf ("cannot get phdrs: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (phnum > 0)
    {
      if (gelf_newphdr (elfb, phnum) == 0)
	{
	  printf ("cannot create phdrs: %s\n", elf_errmsg (-1));
	  exit (1);
	}

      for (size_t cnt = 0; cnt < phnum; ++cnt)
	{
	  GElf_Phdr phdr_mem;
	  GElf_Phdr *phdr = gelf_getphdr (elfa, cnt, &phdr_mem);
	  if (phdr == NULL)
	    {
	      printf ("couldn't get phdr %zd: %s\n", cnt, elf_errmsg (-1));
	      exit (1);
	    }

	  if (gelf_update_phdr (elfb, cnt, phdr) == 0)
	    {
	      printf ("couldn't update phdr %zd: %s\n", cnt, elf_errmsg (-1));
	      exit (1);
	    }
	}
    }

  GElf_Off *offs = NULL;
  size_t shnum;
  if (reverse_offs)
    {
      if (elf_getshdrnum (elfa, &shnum) < 0)
	{
	  printf ("couldn't get shdrnum: %s\n", elf_errmsg (-1));
	  exit (1);
	}

      offs = malloc (shnum * sizeof (GElf_Off));
      if (offs == NULL)
	{
	  printf ("couldn't allocate memory for offs\n");
	  exit (1);
	}
    }

  /* Copy all sections, headers and data.  */
  Elf_Scn *scn = NULL;
  size_t last_off = 0;
  GElf_Shdr last_shdr = { .sh_type = SHT_NULL };
  while ((scn = elf_nextscn (elfa, scn)) != NULL)
    {
      /* Get the header.  */
      GElf_Shdr shdr;
      if (gelf_getshdr (scn, &shdr) == NULL)
	{
	  printf ("couldn't get shdr: %s\n", elf_errmsg (-1));
	  exit (1);
	}

      if (reverse_offs)
	{
	  offs[last_off] = shdr.sh_offset;

	  if (last_shdr.sh_type != SHT_NULL
	      && last_shdr.sh_addralign == shdr.sh_addralign
	      && shdr.sh_addralign == 1
	      && last_shdr.sh_type != SHT_NOBITS
	      && shdr.sh_type != SHT_NOBITS
	      && last_shdr.sh_offset + last_shdr.sh_size == shdr.sh_offset
	      && (phnum == 0
		  || ((shdr.sh_flags & SHF_ALLOC) == 0
		      && (last_shdr.sh_flags & SHF_ALLOC) == 0)))
	    {
	      printf ("Swapping offsets of section %zd and %zd\n",
		      last_off, last_off + 1);
	      GElf_Word off = offs[last_off - 1];
	      offs[last_off - 1] = off + shdr.sh_size;
	      offs[last_off] = off;
	      last_shdr.sh_type = SHT_NULL;
	    }
	  else
	    {
	      last_shdr = shdr;
	      offs[last_off] = shdr.sh_offset;
	    }
	  last_off++;
	}

      /* Create new section.  */
      Elf_Scn *new_scn = elf_newscn (elfb);
      if (new_scn == NULL)
	{
	  printf ("couldn't create new section: %s\n", elf_errmsg (-1));
	  exit (1);
	}

      if (gelf_update_shdr (new_scn, &shdr) == 0)
	{
	  printf ("couldn't update shdr: %s\n", elf_errmsg (-1));
	  exit (1);
	}

      /* Copy over section data.  */
      Elf_Data *data = NULL;
      while ((data = elf_getdata (scn, data)) != NULL)
	{
	  Elf_Data *new_data = elf_newdata (new_scn);
	  if (new_data == NULL)
	    {
	      printf ("couldn't create new section data: %s\n",
		      elf_errmsg (-1));
	      exit (1);
	    }
	  *new_data = *data;
	}
    }

  if (reverse_offs)
    {
      last_off = 0;
      scn = NULL;
      while ((scn = elf_nextscn (elfb, scn)) != NULL)
	{
	  GElf_Shdr shdr;
	  if (gelf_getshdr (scn, &shdr) == NULL)
	    {
	      printf ("couldn't get shdr for updating: %s\n", elf_errmsg (-1));
	      exit (1);
	    }

	  shdr.sh_offset = offs[last_off++];

	  if (gelf_update_shdr (scn, &shdr) == 0)
	    {
	      printf ("couldn't update shdr sh_off: %s\n", elf_errmsg (-1));
	      exit (1);
	    }
	}
      free (offs);
    }

  /* Write everything to disk.  If there are any phdrs, or we want to
     update the offsets, then we want the exact same layout.  Do we
     want ELF_F_PERMISSIVE?  */
  if (phnum > 0 || reverse_offs)
    elf_flagelf (elfb, ELF_C_SET, ELF_F_LAYOUT);
  if (elf_update (elfb, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (elf_end (elfa) != 0)
    {
      printf ("couldn't cleanup elf '%s': %s\n", in, elf_errmsg (-1));
      exit (1);
    }

  if (close (fda) != 0)
    {
      printf ("couldn't close '%s': %s\n", in, strerror (errno));
      exit (1);
    }

  if (elf_end (elfb) != 0)
    {
      printf ("couldn't cleanup elf '%s': %s\n", out, elf_errmsg (-1));
      exit (1);
    }

  if (close (fdb) != 0)
    {
      printf ("couldn't close '%s': %s\n", out, strerror (errno));
      exit (1);
    }
}

int
main (int argc, const char *argv[])
{
  elf_version (EV_CURRENT);

  /* Takes the given file, and create a new identical one.  */
  if (argc < 3 || argc > 5)
    {
      fprintf (stderr, "elfcopy [--mmap] [--reverse-offs] in.elf out.elf\n");
      exit (1);
    }

  int argn = 1;
  bool use_mmap = false;
  if (strcmp (argv[argn], "--mmap") == 0)
    {
      use_mmap = true;
      argn++;
    }

  bool reverse_offs = false;
  if (strcmp (argv[argn], "--reverse-offs") == 0)
    {
      reverse_offs = true;
      argn++;
    }

  const char *in = argv[argn++];
  const char *out = argv[argn];
  copy_elf (in, out, use_mmap, reverse_offs);

  return 0;
}
