/* Test program for elf_newdata function.
   Copyright (C) 2015 Red Hat, Inc.
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

// Random data string (16 bytes).
static char *DATA = "123456789ABCDEF";
static size_t DATA_LEN = 16;

static void
add_section_data (Elf *elf, char *buf, size_t len)
{
  printf ("Adding %zd bytes.\n", len);

  Elf_Scn *scn = elf_getscn (elf, 1);
  if (scn == NULL)
    {
      printf ("couldn't get data section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  Elf_Data *data = elf_newdata (scn);
  if (data == NULL)
    {
      printf ("cannot create newdata for section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  data->d_buf = buf;
  data->d_type = ELF_T_BYTE;
  data->d_size = len;
  data->d_align = 1;
  data->d_version = EV_CURRENT;

  // Let the library compute the internal structure information.
  if (elf_update (elf, ELF_C_NULL) < 0)
    {
      printf ("failure in elf_update(NULL): %s\n", elf_errmsg (-1));
      exit (1);
    }

}

static Elf *
create_elf (int fd, int class, int use_mmap)
{
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
  ehdr->e_ident[EI_DATA] = class == ELFCLASS32 ? ELFDATA2LSB : ELFDATA2MSB;
  ehdr->e_ident[EI_OSABI] = ELFOSABI_GNU;
  ehdr->e_type = ET_NONE;
  ehdr->e_machine = class == ELFCLASS32 ? EM_PPC : EM_X86_64;
  ehdr->e_version = EV_CURRENT;

  // Update the ELF header.
  if (gelf_update_ehdr (elf, ehdr) == 0)
    {
      printf ("cannot update ELF header: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Create a section.
  Elf_Scn *scn = elf_newscn (elf);
  if (scn == NULL)
    {
      printf ("cannot create new section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    {
      printf ("cannot get header for data section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  shdr->sh_type = SHT_PROGBITS;
  shdr->sh_flags = 0;
  shdr->sh_addr = 0;
  shdr->sh_link = SHN_UNDEF;
  shdr->sh_info = SHN_UNDEF;
  shdr->sh_addralign = 1;
  shdr->sh_entsize = 1;
  shdr->sh_name = 0;

  // Finish section, update the header.
  if (gelf_update_shdr (scn, shdr) == 0)
    {
      printf ("cannot update header for DATA section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Add some data to the section.
  add_section_data (elf, DATA, DATA_LEN);

  // Write everything to disk.
  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update(WRITE): %s\n", elf_errmsg (-1));
      exit (1);
    }

  return elf;
}

static Elf *
read_elf (int fd, int use_mmap)
{
  printf ("Reading ELF file\n");
  Elf *elf = elf_begin (fd, use_mmap ? ELF_C_RDWR_MMAP : ELF_C_RDWR, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor read-again: %s\n", elf_errmsg (-1));
      exit (1);
    }

  return elf;
}

static void
check_section_size (Elf *elf, size_t size)
{
  Elf_Scn *scn = elf_getscn (elf, 1);
  if (scn == NULL)
    {
      printf ("couldn't get data section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    {
      printf ("cannot get header for DATA section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (shdr->sh_size == size)
    printf ("OK %zd bytes.\n", size);
  else
    {
      printf ("BAD size, expected %zd, got %" PRIu64 "\n",
	      size, shdr->sh_size);
      exit (-1);
    }
}

static void
check_section_data (Elf *elf, char *data, size_t len, size_t times)
{
  Elf_Scn *scn = elf_getscn (elf, 1);
  if (scn == NULL)
    {
      printf ("couldn't get data section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  Elf_Data *d = NULL;
  for (size_t i = 0; i < times; i++)
    {
      if (d == NULL || i * len >= d->d_off + d->d_size)
	{
	  d = elf_getdata (scn, d);
	  if (d == NULL)
	    {
	      printf ("cannot get data for section item %zd: %s\n",
		      i, elf_errmsg (-1));
	      exit (1);
	    }
	  else
	    printf ("OK, section data item %zd (d_off: %" PRId64
		    ", d_size: %zd)\n", i, d->d_off, d->d_size);
	}
      char *d_data = (char *) d->d_buf + (len * i) - d->d_off;
      printf ("%zd data (d_off: %" PRId64
	      ", len * i: %zd): (%p + %" PRId64 ") %s\n",
	      i, d->d_off, len * i, d->d_buf, (len * i) - d->d_off, d_data);
      if (memcmp (data, d_data, len) != 0)
	{
	  printf ("Got bad data in section for item %zd.\n", i);
	  exit (1);
	}
    }
}

static void
check_elf (int class, int use_mmap)
{
  static const char *fname;
  if (class == ELFCLASS32)
    fname = use_mmap ? "newdata.elf32.mmap" : "newdata.elf32";
  else
    fname = use_mmap ? "newdata.elf64.mmap" : "newdata.elf64";

  printf ("\ncheck_elf: %s\n", fname);

  int fd = open (fname, O_RDWR|O_CREAT|O_TRUNC, DEFFILEMODE);
  if (fd == -1)
    {
      printf ("cannot create `%s': %s\n", fname, strerror (errno));
      exit (1);
    }

  Elf *elf = create_elf (fd, class, use_mmap);
  check_section_size (elf, DATA_LEN);
  check_section_data (elf, DATA, DATA_LEN, 1);

  // Add some more data (won't be written to disk).
  add_section_data (elf, DATA, DATA_LEN);
  check_section_size (elf, 2 * DATA_LEN);
  check_section_data (elf, DATA, DATA_LEN, 2);

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  // Read the ELF from disk now.  And add new data directly.
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open `%s' read-only: %s\n", fname, strerror (errno));
      exit (1);
    }

  elf = read_elf (fd, use_mmap);
  check_section_size (elf, DATA_LEN);
  // But don't check contents, that would read the data...

  // Add some more data.
  add_section_data (elf, DATA, DATA_LEN);
  check_section_size (elf, 2 * DATA_LEN);
  check_section_data (elf, DATA, DATA_LEN, 2);

  // And some more.
  add_section_data (elf, DATA, DATA_LEN);
  check_section_size (elf, 3 * DATA_LEN);
  check_section_data (elf, DATA, DATA_LEN, 3);

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  // Read the ELF from disk now.  And add new data after raw reading.
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open `%s' read-only: %s\n", fname, strerror (errno));
      exit (1);
    }

  elf = read_elf (fd, use_mmap);
  check_section_size (elf, DATA_LEN);
  // But don't check contents, that would read the data...

  // Get raw data before adding new data.
  Elf_Scn *scn = elf_getscn (elf, 1);
  if (scn == NULL)
    {
      printf ("couldn't get data section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  printf ("elf_rawdata\n");
  Elf_Data *data = elf_rawdata (scn, NULL);
  if (data == NULL)
    {
      printf ("couldn't get raw data from section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (data->d_size != DATA_LEN)
    {
      printf ("Unexpected Elf_Data: %zd", data->d_size);
      exit (1);
    }

  // Now add more data.
  add_section_data (elf, DATA, DATA_LEN);
  check_section_size (elf, 2 * DATA_LEN);
  check_section_data (elf, DATA, DATA_LEN, 2);

  // And some more.
  add_section_data (elf, DATA, DATA_LEN);
  check_section_size (elf, 3 * DATA_LEN);
  check_section_data (elf, DATA, DATA_LEN, 3);

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  // Read the ELF from disk now.  And add new data after data reading.
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open `%s' read-only: %s\n", fname, strerror (errno));
      exit (1);
    }

  elf = read_elf (fd, use_mmap);
  check_section_size (elf, DATA_LEN);
  // Get (converted) data before adding new data.
  check_section_data (elf, DATA, DATA_LEN, 1);

  printf ("elf_getdata\n");

  // Now add more data.
  add_section_data (elf, DATA, DATA_LEN);
  check_section_size (elf, 2 * DATA_LEN);
  check_section_data (elf, DATA, DATA_LEN, 2);

  // And some more.
  add_section_data (elf, DATA, DATA_LEN);
  check_section_size (elf, 3 * DATA_LEN);
  check_section_data (elf, DATA, DATA_LEN, 3);

  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      exit (1);
    }

  close (fd);

  unlink (fname);
}

int
main (int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
  // Initialize libelf.
  elf_version (EV_CURRENT);

  // Fill holes with something non-zero to more easily spot bad data.
  elf_fill ('X');

  check_elf (ELFCLASS32, 0);
  check_elf (ELFCLASS32, 1);
  check_elf (ELFCLASS64, 0);
  check_elf (ELFCLASS64, 1);

  return 0;
}
