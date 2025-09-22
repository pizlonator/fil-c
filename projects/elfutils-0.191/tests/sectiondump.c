/* Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "system.h"


/* Prototypes for local functions.  */
static int handle_section (Elf *elf, Elf_Scn *scn);
static void print_bytes (Elf_Data *data);
static void print_symtab (Elf *elf, Elf_Data *data);


int
main (int argc, char *argv[])
{
  Elf *elf;
  int fd;
  int cnt;

  if (argc <= 1)
    exit (1);

  /* Open the test file.  This is given as the first parameter to the
     program.  */
  fd = open (argv[1], O_RDONLY);
  if (fd == -1)
    error (EXIT_FAILURE, errno, "cannot open input file `%s'", argv[1]);

  /* Set the library version we expect.  */
  elf_version (EV_CURRENT);

  /* Create the ELF descriptor.  */
  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    error (EXIT_FAILURE, 0, "cannot create ELF descriptor: %s",
	   elf_errmsg (0));

  /* Now process all the sections mentioned in the rest of the command line.  */
  for (cnt = 2; cnt < argc; ++cnt)
    if (handle_section (elf, elf_getscn (elf, atoi (argv[cnt]))) != 0)
      /* When we encounter an error stop immediately.  */
      error (EXIT_FAILURE, 0, "while processing section %d: %s", cnt,
	   elf_errmsg (0));

  /* Close the descriptor.  */
  if (elf_end (elf) != 0)
    error (EXIT_FAILURE, 0, "failure while closing ELF descriptor: %s",
	   elf_errmsg (0));

  return 0;
}


static int
handle_section (Elf *elf, Elf_Scn *scn)
{
  GElf_Ehdr *ehdr;
  GElf_Ehdr ehdr_mem;
  GElf_Shdr *shdr;
  GElf_Shdr shdr_mem;
  Elf_Data *data;

  /* First get the ELF and section header.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (ehdr == NULL || shdr == NULL)
    return 1;

  /* Print the information from the ELF section header.   */
  printf ("name      = %s\n"
	  "type      = %" PRId32 "\n"
	  "flags     = %" PRIx64 "\n"
	  "addr      = %" PRIx64 "\n"
	  "offset    = %" PRIx64 "\n"
	  "size      = %" PRId64 "\n"
	  "link      = %" PRId32 "\n"
	  "info      = %" PRIx32 "\n"
	  "addralign = %" PRIx64 "\n"
	  "entsize   = %" PRId64 "\n",
	  elf_strptr (elf, ehdr->e_shstrndx, shdr->sh_name),
	  shdr->sh_type,
	  shdr->sh_flags,
	  shdr->sh_addr,
	  shdr->sh_offset,
	  shdr->sh_size,
	  shdr->sh_link,
	  shdr->sh_info,
	  shdr->sh_addralign,
	  shdr->sh_entsize);

  /* Get the section data now.  */
  data = elf_getdata (scn, NULL);
  if (data == NULL)
    return 1;

  /* Now process the different section types accordingly.  */
  switch (shdr->sh_type)
    {
    case SHT_SYMTAB:
      print_symtab (elf, data);
      break;

    case SHT_PROGBITS:
    default:
      print_bytes (data);
      break;
    }

  /* Separate form the next section.  */
  puts ("");

  /* All done correctly.  */
  return 0;
}


static void
print_bytes (Elf_Data *data)
{
  size_t size = data->d_size;
  off_t offset = data->d_off;
  unsigned char *buf = (unsigned char *) data->d_buf;
  size_t cnt;

  for (cnt = 0; cnt < size; cnt += 16)
    {
      size_t inner;

      printf ("%*zx: ", sizeof (size_t) == 4 ? 8 : 16, (size_t) offset + cnt);

      for (inner = 0; inner < 16 && cnt + inner < size; ++inner)
	printf (" %02hhx", buf[cnt + inner]);

      puts ("");
    }
}


static void
print_symtab (Elf *elf, Elf_Data *data)
{
  int class = gelf_getclass (elf);
  size_t nsym = data->d_size / (class == ELFCLASS32
				? sizeof (Elf32_Sym) : sizeof (Elf64_Sym));
  size_t cnt;

  for (cnt = 0; cnt < nsym; ++cnt)
    {
      GElf_Sym sym_mem;
      GElf_Sym *sym = gelf_getsym (data, cnt, &sym_mem);

      printf ("%5zu: %*" PRIx64 " %6" PRIx64 " %4d\n",
	      cnt,
	      class == ELFCLASS32 ? 8 : 16,
	      sym->st_value,
	      sym->st_size,
	      GELF_ST_TYPE (sym->st_info));
    }
}
