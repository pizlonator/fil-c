/* Test program for adding a program header to a vendor specific ELF file.
   Copyright (C) 2016 Red Hat, Inc.
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
#include "system.h"

#include ELFUTILS_HEADER(elf)
#include <gelf.h>

void
check_elf (const char *fname, int class, int use_mmap)
{
  printf ("\nfname: %s\n", fname);

  int fd = open (fname, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE);
  if (fd == -1)
    {
      printf ("cannot open `%s': %s\n", fname, strerror (errno));
      exit (1);
    }

  Elf *elf = elf_begin (fd, use_mmap ? ELF_C_WRITE_MMAP : ELF_C_WRITE, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Create an ELF header.
  if (gelf_newehdr (elf, class) == 0)
    {
      printf ("cannot create ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      printf ("cannot get ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Initialize header.
  ehdr->e_ident[EI_DATA] = class == ELFCLASS64 ? ELFDATA2LSB : ELFDATA2MSB;
  ehdr->e_ident[EI_OSABI] = ELFOSABI_GNU;
  ehdr->e_type = ET_LOOS + 1;
  ehdr->e_machine = EM_X86_64;
  ehdr->e_version = EV_CURRENT;

  if (gelf_update_ehdr (elf, ehdr) == 0)
    {
      printf ("cannot update ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Create a program header.
  if (gelf_newphdr (elf, 1) == 0)
    {
      printf ("cannot create program header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  GElf_Phdr phdr;
  if (gelf_getphdr (elf, 0, &phdr) == NULL)
    {
      printf ("cannot get program header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Some random values to check later.
  phdr.p_type = PT_NULL;
  phdr.p_offset = 0;
  phdr.p_vaddr = 0;
  phdr.p_paddr = 1;
  phdr.p_filesz = 0;
  phdr.p_memsz = 1024;
  phdr.p_flags = PF_R;
  phdr.p_align = 16;

  if (gelf_update_phdr (elf, 0, &phdr) == 0)
    {
      printf ("cannot update program header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Write everything to disk.
  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update(WRITE): %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  /* Reread the ELF from disk now.  */
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open `%s' read-only: %s\n", fname, strerror (errno));
      exit (1);
    }

  elf = elf_begin (fd, use_mmap ? ELF_C_READ_MMAP : ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor read-only: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Is our phdr there?
  size_t phnum;
  if (elf_getphdrnum (elf, &phnum) != 0)
    {
      printf ("cannot get phdr num: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (phnum != 1)
    {
      printf ("Expected just 1 phdr, got: %zd\n", phnum);
      exit (1);
    }

  if (gelf_getphdr (elf, 0, &phdr) == NULL)
    {
      printf ("cannot get program header from file: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (phdr.p_type != PT_NULL
      || phdr.p_offset != 0
      || phdr.p_vaddr != 0
      || phdr.p_paddr != 1
      || phdr.p_filesz != 0
      || phdr.p_memsz != 1024
      || phdr.p_flags != PF_R
      || phdr.p_align != 16)
    {
      printf ("Unexpected phdr values\n");
      exit (1);
    }

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  unlink (fname);
}

int
main (int argc __attribute__ ((unused)),
      char *argv[] __attribute__ ((unused)))
{
  elf_version (EV_CURRENT);

  check_elf ("vendor.elf.32", ELFCLASS32, 0);
  check_elf ("vendor.elf.32.mmap", ELFCLASS32, 1);
  check_elf ("vendor.elf.64", ELFCLASS64, 0);
  check_elf ("vendor.elf.64.mmap", ELFCLASS64, 1);

  return 0;
}
