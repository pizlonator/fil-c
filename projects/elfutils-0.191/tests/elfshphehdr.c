/* Test program for adding section and program headers and ehdr updates.
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

#include <config.h>
#include <assert.h>
#include ELFUTILS_HEADER(elf)
#include <gelf.h>

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdbool.h>

void
check (const char *msg, bool statement)
{
  if (! statement)
    {
      fprintf (stderr, "%s FAILED\n", msg);
      exit (-1);
    }
  else
    fprintf (stderr, "%s OK\n", msg);
}

void
check_elf (const char *msg, bool statement)
{
  if (! statement)
    {
      fprintf (stderr, "%s: %s\n", msg, elf_errmsg (-1));
      exit (-1);
    }
  else
    fprintf (stderr, "%s OK\n", msg);
}

void
test (Elf *elf, int class, bool layout)
{
  fprintf (stderr, "testing ELF class: %d, layout: %d\n", class, layout);

  check_elf ("gelf_newehdr", gelf_newehdr (elf, class) != 0);
  check_elf ("gelf_getclass", gelf_getclass (elf) == class);

  check_elf ("elf_flagelf", elf_flagelf (elf, layout ? ELF_C_SET : ELF_C_CLR,
					 ELF_F_LAYOUT) != 0);

  GElf_Ehdr ehdr;
  check_elf ("gelf_getehdr", gelf_getehdr (elf, &ehdr) != NULL);
  check ("e_shnum == 0", ehdr.e_shnum == 0);
  check ("e_phnum == 0", ehdr.e_phnum == 0);
  check ("e_shoff == 0", ehdr.e_shoff == 0);
  check ("e_phoff == 0", ehdr.e_phoff == 0);

  size_t shnum;
  check_elf ("elf_getshdrnum", elf_getshdrnum (elf, &shnum) == 0);
  check ("shnum == 0", shnum == 0);

  size_t phnum;
  check_elf ("elf_getphdrnum", elf_getphdrnum (elf, &phnum) == 0);
  check ("phnum == 0", phnum == 0);

  /* Lets fill in some info we are always responsible for.  */
  ehdr.e_ident[EI_DATA] = ELFDATANONE; /* Ask for native encoding.  */
  ehdr.e_type = ET_EXEC;
  ehdr.e_machine = EM_386;
  ehdr.e_version = EV_NONE; /* Ask for current version. */
  check_elf ("gelf_update_ehdr", gelf_update_ehdr (elf, &ehdr) != 0);

  check_elf ("elf_update", elf_update (elf, ELF_C_NULL) > 0);

  check_elf ("gelf_getehdr", gelf_getehdr (elf, &ehdr) != NULL);
  check ("EI_DATA", ehdr.e_ident[EI_DATA] != ELFDATANONE);
  check ("e_version", ehdr.e_version == EV_CURRENT);

  /* The sh/ph values shouldn't have changed.  */
  check ("e_shnum == 0", ehdr.e_shnum == 0);
  check ("e_phnum == 0", ehdr.e_phnum == 0);
  check ("e_shoff == 0", ehdr.e_shoff == 0);
  check ("e_phoff == 0", ehdr.e_phoff == 0);

  check_elf ("elf_getshdrnum", elf_getshdrnum (elf, &shnum) == 0);
  check ("shnum == 0", shnum == 0);

  check_elf ("elf_getphdrnum", elf_getphdrnum (elf, &phnum) == 0);
  check ("phnum == 0", phnum == 0);

  /* Lets add a header.  */
  check_elf ("elf_newscn", elf_newscn (elf) != NULL);
  check_elf ("gelf_newphdr", gelf_newphdr (elf, 1) != 0);

  /* If we are responsible for the layout ourselves we should also
     tell where to put them.  */
  if (layout)
    {
      check_elf ("gelf_getehdr", gelf_getehdr (elf, &ehdr) != NULL);
      /* phdrs go right after the ehdr.  */
      ehdr.e_phoff = ehdr.e_ehsize;
      /* shdrs go right after the phdrs.  */
      ehdr.e_shoff = ehdr.e_phoff + ehdr.e_phnum * ehdr.e_phentsize;
      check_elf ("gelf_update_ehdr", gelf_update_ehdr (elf, &ehdr) != 0);
    }

  check_elf ("elf_update", elf_update (elf, ELF_C_NULL) > 0);

  check_elf ("elf_getshdrnum", elf_getshdrnum (elf, &shnum) == 0);
  check ("shnum == 1", shnum == 2); /* section zero is also created.  */

  check_elf ("elf_getphdrnum", elf_getphdrnum (elf, &phnum) == 0);
  check ("phnum == 1", phnum == 1);

  check_elf ("gelf_getehdr", gelf_getehdr (elf, &ehdr) != NULL);

  check ("EI_DATA", ehdr.e_ident[EI_DATA] != ELFDATANONE);
  check ("e_version", ehdr.e_version == EV_CURRENT);

  check ("e_shnum == 2", ehdr.e_shnum == 2);
  check ("e_phnum == 1", ehdr.e_phnum == 1);
  check ("e_shoff != 0", ehdr.e_shoff != 0);
  check ("e_phoff != 0", ehdr.e_phoff != 0);

  size_t shentsize = (class == ELFCLASS32
		      ? sizeof (Elf32_Shdr) : sizeof (Elf64_Shdr));
  check ("e_shentsize", ehdr.e_shentsize == shentsize);
  size_t phentsize = (class == ELFCLASS32
		      ? sizeof (Elf32_Phdr) : sizeof (Elf64_Phdr));
  check ("e_phentsize", ehdr.e_phentsize == phentsize);
}

int
main (int argc __attribute__ ((unused)), char **argv __attribute ((unused)))
{
  elf_version (EV_CURRENT);

  int fd = open("/dev/null", O_WRONLY);
  check ("open", fd >= 0);

  Elf *elf;

  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  check_elf ("elf_begin", elf != NULL);
  test (elf, ELFCLASS32, false);
  elf_end (elf);

  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  check_elf ("elf_begin", elf != NULL);
  test (elf, ELFCLASS32, true);
  elf_end (elf);

  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  check_elf ("elf_begin", elf != NULL);
  test (elf, ELFCLASS64, false);
  elf_end (elf);

  elf = elf_begin (fd, ELF_C_WRITE, NULL);
  check_elf ("elf_begin", elf != NULL);
  test (elf, ELFCLASS64, true);
  elf_end (elf);

  close (fd);
  return 0;
}
