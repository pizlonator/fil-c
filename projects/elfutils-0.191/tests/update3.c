/* Test program for elf_update function.
   Copyright (C) 2000, 2002, 2005, 2016 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "system.h"

#include ELFUTILS_HEADER(dwelf)


int
main (int argc, char *argv[] __attribute__ ((unused)))
{
  const char *fname = "xxx_update3";
  int fd;
  Elf *elf;
  Elf32_Ehdr *ehdr;
  Elf32_Phdr *phdr;
  Elf_Scn *scn;
  Elf32_Shdr *shdr;
  Elf_Data *data;
  Dwelf_Strtab *shst;
  Dwelf_Strent *shstrtabse;
  int i;

  fd = open (fname, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE);
  if (fd == -1)
    {
      printf ("cannot open `%s': %s\n", fname, strerror (errno));
      exit (1);
    }

  elf_version (EV_CURRENT);

  elf_fill (0x42);

  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Create an ELF header.  */
  ehdr = elf32_newehdr (elf);
  if (ehdr == NULL)
    {
      printf ("cannot create ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* Print the ELF header values.  */
  if (argc > 1)
    {
      for (i = 0; i < EI_NIDENT; ++i)
	printf (" %02x", ehdr->e_ident[i]);
      printf ("\
\ntype = %hu\nmachine = %hu\nversion = %u\nentry = %u\nphoff = %u\n"
	      "shoff = %u\nflags = %u\nehsize = %hu\nphentsize = %hu\n"
	      "phnum = %hu\nshentsize = %hu\nshnum = %hu\nshstrndx = %hu\n",
	      ehdr->e_type, ehdr->e_machine, ehdr->e_version, ehdr->e_entry,
	      ehdr->e_phoff, ehdr->e_shoff, ehdr->e_flags, ehdr->e_ehsize,
	      ehdr->e_phentsize, ehdr->e_phnum, ehdr->e_shentsize,
	      ehdr->e_shnum, ehdr->e_shstrndx);
    }

  ehdr->e_ident[0] = 42;
  ehdr->e_ident[4] = 1;
  ehdr->e_ident[5] = 1;
  ehdr->e_ident[6] = 2;
  ehdr->e_type = ET_EXEC;
  ehdr->e_version = 1;
  ehdr->e_ehsize = 1;
  elf_flagehdr (elf, ELF_C_SET, ELF_F_DIRTY);

  /* Create the program header.  */
  phdr = elf32_newphdr (elf, 1);
  if (phdr == NULL)
    {
      printf ("cannot create program header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  phdr[0].p_type = PT_PHDR;
  elf_flagphdr (elf, ELF_C_SET, ELF_F_DIRTY);

  shst = dwelf_strtab_init (true);

  scn = elf_newscn (elf);
  if (scn == NULL)
    {
      printf ("cannot create SHSTRTAB section: %s\n", elf_errmsg (-1));
      exit (1);
    }
  shdr = elf32_getshdr (scn);
  if (shdr == NULL)
    {
      printf ("cannot get header for SHSTRTAB section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  shstrtabse = dwelf_strtab_add (shst, ".shstrtab");

  shdr->sh_type = SHT_STRTAB;
  shdr->sh_flags = 0;
  shdr->sh_addr = 0;
  shdr->sh_link = SHN_UNDEF;
  shdr->sh_info = SHN_UNDEF;
  shdr->sh_addralign = 1;
  shdr->sh_entsize = 0;

  /* We have to store the section index in the ELF header.  */
  ehdr->e_shstrndx = elf_ndxscn (scn);

  data = elf_newdata (scn);
  if (data == NULL)
    {
      printf ("cannot create data SHSTRTAB section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* No more sections, finalize the section header string table.  */
  dwelf_strtab_finalize (shst, data);

  shdr->sh_name = dwelf_strent_off (shstrtabse);

  /* Let the library compute the internal structure information.  */
  if (elf_update (elf, ELF_C_NULL) < 0)
    {
      printf ("failure in elf_update(NULL): %s\n", elf_errmsg (-1));
      exit (1);
    }

  ehdr = elf32_getehdr (elf);

  phdr[0].p_offset = ehdr->e_phoff;
  phdr[0].p_offset = ehdr->e_phoff;
  phdr[0].p_vaddr = ehdr->e_phoff;
  phdr[0].p_paddr = ehdr->e_phoff;
  phdr[0].p_flags = PF_R | PF_X;
  phdr[0].p_filesz = ehdr->e_phnum * elf32_fsize (ELF_T_PHDR, 1, EV_CURRENT);
  phdr[0].p_memsz = ehdr->e_phnum * elf32_fsize (ELF_T_PHDR, 1, EV_CURRENT);
  phdr[0].p_align = sizeof (Elf32_Word);

  /* Write out the file.  */
  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update(WRITE): %s\n", elf_errmsg (-1));
      exit (1);
    }

  /* We don't need the string table anymore.  */
  dwelf_strtab_free (shst);

  /* And the data allocated in the .shstrtab section.  */
  free (data->d_buf);

  /* Print the ELF header values.  */
  if (argc > 1)
    {
      for (i = 0; i < EI_NIDENT; ++i)
	printf (" %02x", ehdr->e_ident[i]);
      printf ("\
\ntype = %hu\nmachine = %hu\nversion = %u\nentry = %u\nphoff = %u\n"
	      "shoff = %u\nflags = %u\nehsize = %hu\nphentsize = %hu\n"
	      "phnum = %hu\nshentsize = %hu\nshnum = %hu\nshstrndx = %hu\n",
	      ehdr->e_type, ehdr->e_machine, ehdr->e_version, ehdr->e_entry,
	      ehdr->e_phoff, ehdr->e_shoff, ehdr->e_flags, ehdr->e_ehsize,
	      ehdr->e_phentsize, ehdr->e_phnum, ehdr->e_shentsize,
	      ehdr->e_shnum, ehdr->e_shstrndx);
    }

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  unlink (fname);

  return 0;
}
