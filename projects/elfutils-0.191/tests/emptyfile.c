/* Test program for adding a section to an empty ELF file.
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


/* Index of last string added.  Returned by add_string ().  */
static size_t stridx = 0;

/* Adds a string and returns the offset in the section.  */
static size_t
add_string (Elf_Scn *scn, char *str)
{
  size_t lastidx = stridx;
  size_t size = strlen (str) + 1;

  Elf_Data *data = elf_newdata (scn);
  if (data == NULL)
    {
      printf ("cannot create data SHSTRTAB section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  data->d_buf = str;
  data->d_type = ELF_T_BYTE;
  data->d_size = size;
  data->d_align = 1;
  data->d_version = EV_CURRENT;

  stridx += size;
  printf ("add_string: '%s', stridx: %zd, lastidx: %zd\n",
	  str, stridx, lastidx);
  return lastidx;
}

static void
check_elf (const char *fname, int class, int use_mmap)
{
  printf ("\nfname: %s\n", fname);
  stridx = 0; // Reset strtab strings index

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
  ehdr->e_type = ET_NONE;
  ehdr->e_machine = EM_X86_64;
  ehdr->e_version = EV_CURRENT;

  if (gelf_update_ehdr (elf, ehdr) == 0)
    {
      printf ("cannot update ELF header: %s\n", elf_errmsg (-1));
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
  fd = open (fname, O_RDWR);
  if (fd == -1)
    {
      printf ("cannot (re)open `%s': %s\n", fname, strerror (errno));
      exit (1);
    }

  elf = elf_begin (fd, use_mmap ? ELF_C_RDWR_MMAP : ELF_C_RDWR, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor read-only: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // There are no sections yet.
  if (elf_nextscn (elf, NULL) != NULL)
    {
      printf ("Empty elf had a section???\n");
      exit (1);
    }

  // Create strtab section.
  Elf_Scn *scn = elf_newscn (elf);
  if (scn == NULL)
    {
      printf ("cannot create strings section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Add an empty string to the table as NUL entry for section zero.
  add_string (scn, "");

  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    {
      printf ("cannot get header for strings section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  shdr->sh_type = SHT_STRTAB;
  shdr->sh_flags = 0;
  shdr->sh_addr = 0;
  shdr->sh_link = SHN_UNDEF;
  shdr->sh_info = SHN_UNDEF;
  shdr->sh_addralign = 1;
  shdr->sh_entsize = 0;
  shdr->sh_name = add_string (scn, ".strtab");

  // We have to store the section strtab index in the ELF header.
  // So sections have actual names.
  int ndx = elf_ndxscn (scn);
  ehdr->e_shstrndx = ndx;

  if (gelf_update_ehdr (elf, ehdr) == 0)
    {
      printf ("cannot update ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Finished strtab section, update the header.
  if (gelf_update_shdr (scn, shdr) == 0)
    {
      printf ("cannot update STRTAB section header: %s\n", elf_errmsg (-1));
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

  // And read it in one last time.
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

  // Is our new section there?
  scn = elf_nextscn (elf, NULL);
  if (scn == NULL)
    {
      printf ("cannot get new section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    {
      printf ("cannot get header for new section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx) < 0)
    {
      printf ("elf_getshdrstrndx: %s\n", elf_errmsg (-1));
      exit (1);
    }

  const char *sname = elf_strptr (elf, shstrndx, shdr->sh_name);
  if (sname == NULL || strcmp (sname, ".strtab") != 0)
    {
      printf ("Bad section name: %s\n", sname);
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

  check_elf ("empty.elf.32", ELFCLASS32, 0);
  check_elf ("empty.elf.32.mmap", ELFCLASS32, 1);
  check_elf ("empty.elf.64", ELFCLASS64, 0);
  check_elf ("empty.elf.64.mmap", ELFCLASS64, 1);

  return 0;
}
