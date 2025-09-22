/* Create an ELF file with all the DT_* flags set.
   Copyright (C) 2011, 2016 Red Hat, Inc.
   This file is part of elfutils.
   Written by Marek Polacek <mpolacek@redhat.com>, 2011.

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

#include ELFUTILS_HEADER(dwelf)
#include <elf.h>
#include <gelf.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "system.h"


int
main (void)
{
  static const char fname[] = "testfile-alldts";
  Dwelf_Strtab *shst;
  Dwelf_Strent *dynscn;
  Dwelf_Strent *shstrtabse;
  const Elf32_Sword dtflags[] =
    {
      DT_NEEDED, DT_PLTRELSZ, DT_PLTGOT,
      DT_HASH, DT_STRTAB, DT_SYMTAB, DT_RELA,
      DT_RELASZ, DT_RELAENT, DT_STRSZ, DT_SYMENT,
      DT_INIT, DT_FINI, DT_SONAME, DT_RPATH,
      DT_SYMBOLIC, DT_REL, DT_RELSZ, DT_RELENT,
      DT_PLTREL, DT_DEBUG, DT_TEXTREL, DT_JMPREL,
      DT_BIND_NOW, DT_INIT_ARRAY, DT_FINI_ARRAY,
      DT_INIT_ARRAYSZ, DT_FINI_ARRAYSZ, DT_RUNPATH,
      DT_FLAGS, DT_ENCODING, DT_PREINIT_ARRAY,
      DT_PREINIT_ARRAYSZ, DT_VERSYM, DT_GNU_PRELINKED,
      DT_GNU_CONFLICTSZ, DT_GNU_LIBLISTSZ, DT_CHECKSUM,
      DT_PLTPADSZ, DT_MOVEENT, DT_MOVESZ, DT_FEATURE_1,
      DT_POSFLAG_1, DT_SYMINSZ, DT_SYMINENT, DT_GNU_HASH,
      DT_TLSDESC_PLT, DT_TLSDESC_GOT, DT_GNU_CONFLICT,
      DT_GNU_LIBLIST, DT_CONFIG, DT_DEPAUDIT, DT_AUDIT,
      DT_PLTPAD, DT_MOVETAB, DT_SYMINFO, DT_RELACOUNT,
      DT_RELCOUNT, DT_FLAGS_1, DT_VERDEF, DT_VERDEFNUM,
      DT_VERNEED, DT_VERNEEDNUM, DT_AUXILIARY, DT_FILTER,
      DT_NULL
    };
  const int ndtflags = sizeof (dtflags) / sizeof (dtflags[0]);

  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Open the file.  */
  int fd = open (fname, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE);
  if (fd == -1)
    {
      printf ("cannot open `%s': %m\n", fname);
      return 1;
    }

  /* Tell the library which version are we expecting.  */
  elf_version (EV_CURRENT);

  /* Create an ELF descriptor.  */
  Elf *elf = elf_begin (fd, ELF_C_WRITE, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      return 1;
    }

  /* Create an ELF header.  */
  Elf32_Ehdr *ehdr = elf32_newehdr (elf);
  if (ehdr == NULL)
    {
      printf ("cannot create ELF header: %s\n", elf_errmsg (-1));
      return 1;
    }

  ehdr->e_ident[0] = 42;
  ehdr->e_ident[5] = 1;
  ehdr->e_ident[6] = 2;
  ehdr->e_type = ET_EXEC;
  ehdr->e_machine = EM_386;
  ehdr->e_version = 1;
  ehdr->e_ehsize = 1;
  ehdr->e_shnum = 3;

  elf_flagehdr (elf, ELF_C_SET, ELF_F_DIRTY);

  /* Create the program headers.  */
  Elf32_Phdr *phdr = elf32_newphdr (elf, 2);
  if (phdr == NULL)
    {
      printf ("cannot create program headers: %s\n", elf_errmsg (-1));
      return 1;
    }

  phdr[0].p_type = PT_PHDR;
  phdr[1].p_type = PT_DYNAMIC;

  elf_flagphdr (elf, ELF_C_SET, ELF_F_DIRTY);
  shst = dwelf_strtab_init (true);

  /* Create the .dynamic section.  */
  Elf_Scn *scn = elf_newscn (elf);
  if (scn == NULL)
    {
      printf ("cannot create DYNAMIC section: %s\n", elf_errmsg (-1));
      return 1;
    }

  Elf32_Shdr *shdr = elf32_getshdr (scn);
  if (shdr == NULL)
    {
      printf ("cannot get header for DYNAMIC section: %s\n", elf_errmsg (-1));
      return 1;
    }

  dynscn = dwelf_strtab_add (shst, ".dynamic");

  /* We'll need to know the section offset.  But this will be set up
     by elf_update later, so for now just store the address.  */
  const Elf32_Off *const dynscn_offset = &shdr->sh_offset;
  shdr->sh_type = SHT_DYNAMIC;
  shdr->sh_flags = SHF_ALLOC | SHF_WRITE;
  shdr->sh_link = SHN_UNDEF;
  shdr->sh_info = SHN_UNDEF;
  /* This section will start here.  */
  shdr->sh_addr = 0x1a0;

  /* Create new section data.  */
  Elf_Data *data = elf_newdata (scn);
  if (data == NULL)
    {
      printf ("cannot create data for DYNAMIC section: %s\n", elf_errmsg (-1));
      return 1;
    }

  /* Allocate memory for all the .dynamic entries.  */
  Elf32_Dyn *dyn = malloc (ndtflags * sizeof (Elf32_Dyn));
  if (dyn == NULL)
    {
      printf ("malloc failed: %m\n");
      return 1;
    }

  /* Now write all the DT_* flags.  */
  for (int i = 0; i < ndtflags; ++i)
    {
      dyn[i].d_tag = dtflags[i];
      dyn[i].d_un.d_val = 0xdeadbeef;
    }

  /* Set the pointer to allocated memory.  */
  data->d_buf = dyn;
  data->d_type = ELF_T_DYN;
  data->d_version = EV_CURRENT;
  data->d_size = ndtflags * sizeof (Elf32_Dyn);
  data->d_align = 0x8;

  /* Create .shstrtab section.  */
  scn = elf_newscn (elf);
  if (scn == NULL)
    {
      printf ("cannot create SHSTRTAB section: %s\n", elf_errmsg (-1));
      return 1;
    }

  shdr = elf32_getshdr (scn);
  if (shdr == NULL)
    {
      printf ("cannot get header for SHSTRTAB section: %s\n", elf_errmsg (-1));
      return 1;
    }

  shstrtabse = dwelf_strtab_add (shst, ".shstrtab");

  shdr->sh_type = SHT_STRTAB;
  shdr->sh_flags = 0;
  shdr->sh_addr = 0;
  shdr->sh_link = SHN_UNDEF;
  shdr->sh_info = SHN_UNDEF;
  shdr->sh_entsize = 1;

  /* We have to store the section index in the ELF header.  */
  ehdr->e_shstrndx = elf_ndxscn (scn);

  data = elf_newdata (scn);
  if (data == NULL)
    {
      printf ("cannot create data SHSTRTAB section: %s\n", elf_errmsg (-1));
      return 1;
    }

  /* No more sections, finalize the section header string table.  */
  dwelf_strtab_finalize (shst, data);

  elf32_getshdr (elf_getscn (elf, 1))->sh_name = dwelf_strent_off (dynscn);
  shdr->sh_name = dwelf_strent_off (shstrtabse);

  /* Let the library compute the internal structure information.  */
  if (elf_update (elf, ELF_C_NULL) < 0)
    {
      printf ("failure in elf_update(NULL): %s\n", elf_errmsg (-1));
      return 1;
    }

  ehdr = elf32_getehdr (elf);

  phdr[0].p_offset = ehdr->e_phoff;
  phdr[0].p_vaddr = ehdr->e_phoff;
  phdr[0].p_paddr = ehdr->e_phoff;
  phdr[0].p_flags = PF_R | PF_X;
  phdr[0].p_filesz = ehdr->e_phnum * elf32_fsize (ELF_T_PHDR, 1, EV_CURRENT);
  phdr[0].p_memsz = ehdr->e_phnum * elf32_fsize (ELF_T_PHDR, 1, EV_CURRENT);
  phdr[0].p_align = sizeof (Elf32_Word);

  phdr[1].p_flags = PF_W | PF_R;
  phdr[1].p_offset = *dynscn_offset;
  /* Set up the start of this segment to equal start address of the
     .dynamic section.  */
  phdr[1].p_vaddr = 0x1a0;
  phdr[1].p_paddr = 0x1a0;
  phdr[1].p_align = 2 * sizeof (Elf32_Word);
  phdr[1].p_filesz = ndtflags * sizeof (Elf32_Dyn);
  phdr[1].p_memsz = ndtflags * sizeof (Elf32_Dyn);

  /* Write out the file.  */
  if (elf_update (elf, ELF_C_WRITE) < 0)
    {
      printf ("failure in elf_update(WRITE): %s\n", elf_errmsg (-1));
      return 1;
    }

  /* We don't need the string table anymore.  */
  dwelf_strtab_free (shst);

  /* And the data allocated in the .shstrtab section.  */
  free (data->d_buf);

  /* And the dynamic entries.  */
  free (dyn);

  /* All done.  */
  if (elf_end (elf) != 0)
    {
      printf ("failure in elf_end: %s\n", elf_errmsg (-1));
      return 1;
    }

  return 0;
}
