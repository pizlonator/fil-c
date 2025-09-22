/* Test program for adding (more than SHN_LORESERVE) sections.
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

/* Will add nr new '.extra' sections and a new '.new_shstrtab' section
   at the end.  */
static void
add_sections (const char *name, size_t nr, int use_mmap, size_t sec_size)
{
  printf ("add_sections '%s': %zd (sec_size: %zd)\n", name, nr, sec_size);

  int fd = open (name, O_RDWR);
  if (fd < 0)
    {
      fprintf (stderr, "Couldn't open file '%s': %s\n",
	       name, strerror (errno));
      exit (1);
    }

  Elf *elf = elf_begin (fd, use_mmap ? ELF_C_RDWR_MMAP : ELF_C_RDWR, NULL);
  if (elf == NULL)
    {
      fprintf (stderr, "Couldn't open ELF file '%s': %s\n",
	       name, elf_errmsg (-1));
      exit (1);
    }

  /* We will add a new shstrtab section with two new names at the end.
     Just get the current shstrtab table and add two entries '.extra'
     and '.old_shstrtab' at the end of the table, so all existing indexes
     are still valid.  */
  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx) < 0)
    {
      printf ("cannot get shstrndx: %s\n", elf_errmsg (-1));
      exit (1);
    }

  Elf_Scn *shstrtab_scn = elf_getscn (elf, shstrndx);
  if (shstrtab_scn == NULL)
    {
      printf ("couldn't get shstrtab scn: %s\n", elf_errmsg (-1));
      exit (1);
    }
  Elf_Data *shstrtab_data = elf_getdata (shstrtab_scn, NULL);
  if (shstrtab_data == NULL)
    {
      printf ("couldn't get shstrtab data: %s\n", elf_errmsg (-1));
      exit (1);
    }
  size_t new_shstrtab_size = (shstrtab_data->d_size
			      + strlen (".extra") + 1
			      + strlen (".old_shstrtab") + 1);
  void *new_shstrtab_buf = malloc (new_shstrtab_size);
  if (new_shstrtab_buf == NULL)
    {
      printf ("couldn't allocate new shstrtab data d_buf\n");
      exit (1);
    }
  memcpy (new_shstrtab_buf, shstrtab_data->d_buf, shstrtab_data->d_size);
  size_t extra_idx = shstrtab_data->d_size;
  size_t old_shstrtab_idx = extra_idx + strlen (".extra") + 1;
  strcpy (new_shstrtab_buf + extra_idx, ".extra");
  strcpy (new_shstrtab_buf + old_shstrtab_idx, ".old_shstrtab");

  /* Change the name of the old shstrtab section, because elflint
     has a strict check on the name/type for .shstrtab.  */
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (shstrtab_scn, &shdr_mem);
  if (shdr == NULL)
    {
      printf ("cannot get header for old shstrtab section: %s\n",
              elf_errmsg (-1));
      exit (1);
    }

  size_t shstrtab_idx = shdr->sh_name;
  shdr->sh_name = old_shstrtab_idx;

  if (gelf_update_shdr (shstrtab_scn, shdr) == 0)
    {
      printf ("cannot update old shstrtab section header: %s\n",
	      elf_errmsg (-1));
      exit (1);
    }

  void *buf;
  size_t bufsz;
  if (sec_size == 0)
    {
      buf = "";
      bufsz = 1;
    }
  else
    {
      buf = malloc (sec_size);
      if (buf == NULL)
	{
	  printf ("cannot allocate buffer data of %zd bytes\n", sec_size);
	  exit (1);
	}
      memset (buf, 0xAA, sec_size);
      bufsz = sec_size;
    }

  // Add lots of .extra sections...
  size_t cnt = 0;
  while (cnt++ < nr)
    {
      Elf_Scn *scn = elf_newscn (elf);
      if (scn == NULL)
	{
	  printf ("cannot create .extra section (%zd): %s\n", cnt,
		  elf_errmsg (-1));
	  exit (1);
	}

      Elf_Data *data = elf_newdata (scn);
      if (data == NULL)
	{
	  printf ("couldn't create new section data (%zd): %s\n", cnt,
		  elf_errmsg (-1));
	  exit (1);
	}

      data->d_size = bufsz;
      data->d_buf = buf;
      data->d_type = ELF_T_BYTE;
      data->d_align = 1;

      shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	{
	  printf ("cannot get header for new section (%zd): %s\n", cnt,
		  elf_errmsg (-1));
	  exit (1);
	}

      shdr->sh_type = SHT_PROGBITS;
      shdr->sh_flags = 0;
      shdr->sh_addr = 0;
      shdr->sh_link = SHN_UNDEF;
      shdr->sh_info = SHN_UNDEF;
      shdr->sh_addralign = 1;
      shdr->sh_entsize = 0;
      shdr->sh_size = data->d_size;
      shdr->sh_name = extra_idx;

      if (gelf_update_shdr (scn, shdr) == 0)
	{
	  printf ("cannot update new section header (%zd): %s\n", cnt,
		  elf_errmsg (-1));
	  exit (1);
	}
    }

  // Create new shstrtab section.
  Elf_Scn *new_shstrtab_scn = elf_newscn (elf);
  if (new_shstrtab_scn == NULL)
    {
      printf ("cannot create new shstrtab section: %s\n", elf_errmsg (-1));
      exit (1);
    }

  Elf_Data *new_shstrtab_data = elf_newdata (new_shstrtab_scn);
  if (new_shstrtab_data == NULL)
    {
      printf ("couldn't create new shstrtab section data: %s\n",
	      elf_errmsg (-1));
      exit (1);
    }

  new_shstrtab_data->d_size = new_shstrtab_size;
  new_shstrtab_data->d_buf = new_shstrtab_buf;
  new_shstrtab_data->d_type = ELF_T_BYTE;
  new_shstrtab_data->d_align = 1;

  shdr = gelf_getshdr (new_shstrtab_scn, &shdr_mem);
  if (shdr == NULL)
    {
      printf ("cannot get header for new shstrtab section: %s\n",
	      elf_errmsg (-1));
      exit (1);
    }

  shdr->sh_type = SHT_STRTAB;
  shdr->sh_flags = 0;
  shdr->sh_addr = 0;
  shdr->sh_link = SHN_UNDEF;
  shdr->sh_info = SHN_UNDEF;
  shdr->sh_addralign = 1;
  shdr->sh_entsize = 0;
  shdr->sh_size = new_shstrtab_size;
  shdr->sh_name = shstrtab_idx;

  // Finished new shstrtab section, update the header.
  if (gelf_update_shdr (new_shstrtab_scn, shdr) == 0)
    {
      printf ("cannot update new shstrtab section header: %s\n",
	      elf_errmsg (-1));
      exit (1);
    }

  // Set it as the new shstrtab section to get the names correct.
  size_t new_shstrndx = elf_ndxscn (new_shstrtab_scn);
  if (setshstrndx (elf, new_shstrndx) < 0)
    {
      printf ("cannot set shstrndx: %s\n", elf_errmsg (-1));
      exit (1);
    }

  // Write everything to disk.
  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update: %s\n", elf_errmsg (-1));
      exit (1);
    }

  if (elf_end (elf) != 0)
    {
      printf ("couldn't cleanup elf '%s': %s\n", name, elf_errmsg (-1));
      exit (1);
    }

  if (close (fd) != 0)
    {
      printf ("couldn't close '%s': %s\n", name, strerror (errno));
      exit (1);
    }

  if (sec_size != 0)
    free (buf);
  free (new_shstrtab_buf);
}

int
main (int argc, char *argv[])
{
  elf_version (EV_CURRENT);

  /* Takes the given file, and adds the given number of sections.
     Optionally using mmap and optionally using a given section size.  */
  if (argc < 3 || argc > 5)
    {
      fprintf (stderr, "addsections [--mmap] nr elf.file [sec_size]\n");
      exit (1);
    }

  int argn = 1;
  bool use_mmap = false;
  if (strcmp (argv[argn], "--mmap") == 0)
    {
      use_mmap = true;
      argn++;
    }

  size_t nr = atoi (argv[argn++]);
  const char *file = argv[argn++];

  size_t sec_size = 0;
  if (argn < argc)
    sec_size = atol (argv[argn++]);

  add_sections (file, nr, use_mmap, sec_size);

  return 0;
}
