/* Copyright (C) 2024 Red Hat, Inc.
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

#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

static void
print_reloc_symnames (Elf *elf, Elf_Scn *scn, GElf_Shdr *shdr, size_t sh_entsize)
{
  int nentries = shdr->sh_size / sh_entsize;

  /* Get the data of the section.  */
  Elf_Data *data = elf_getdata (scn, NULL);
  assert (data != NULL);

  /* Get the symbol table information.  */
  Elf_Scn *symscn = elf_getscn (elf, shdr->sh_link);
  GElf_Shdr symshdr_mem;
  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
  Elf_Data *symdata = elf_getdata (symscn, NULL);
  assert (symshdr != NULL);
  assert (symdata != NULL);

  /* Search for the optional extended section index table.  */
  Elf_Data *xndxdata = NULL;
  int xndxscnidx = elf_scnshndx (scn);
  if (xndxscnidx)
    xndxdata = elf_getdata (elf_getscn (elf, xndxscnidx), NULL);

  /* Get the section header string table index.  */
  size_t shstrndx;
  assert (elf_getshdrstrndx (elf, &shstrndx) >= 0);

  printf("Section: %s\n", elf_strptr (elf, shstrndx, shdr->sh_name));
  for (int cnt = 0; cnt < nentries; ++cnt)
    {
      GElf_Rel relmem;
      GElf_Rel *rel = gelf_getrel (data, cnt, &relmem);


      if (likely (rel != NULL))
        {
          GElf_Sym symmem;
          Elf32_Word xndx;
          GElf_Sym *sym = gelf_getsymshndx (symdata, xndxdata,
                                            GELF_R_SYM (rel->r_info),
                                            &symmem, &xndx);

	  if (sym == NULL)
	    {
	      printf ("<SYM NOT FOUND>\n");
	      continue;
	    }

          if (GELF_ST_TYPE (sym->st_info) != STT_SECTION)
            printf ("%s\n", elf_strptr (elf, symshdr->sh_link, sym->st_name));
          else
            {
              /* This is a relocation against a STT_SECTION symbol.  */
              GElf_Shdr secshdr_mem;
              GElf_Shdr *secshdr;
              secshdr = gelf_getshdr (elf_getscn (elf,
                                                  sym->st_shndx == SHN_XINDEX
                                                  ? xndx : sym->st_shndx),
                                      &secshdr_mem);

	      if (secshdr == NULL)
		printf("<SECTION NOT FOUND>\n");
              else
                printf ("%s\n",
			elf_strptr (elf, shstrndx, secshdr->sh_name));
            }
        }
    }
}

int
main (int argc, char *argv[])
{
  if (argc != 2)
    {
      printf ("Usage: elf_print_reloc_syms FILE\n");
      return -1;
    }

  elf_version (EV_CURRENT);

  int fd = open(argv[1], O_RDONLY);
  assert (fd != -1);

  Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
  assert (elf != NULL);

  size_t shnums;
  assert (elf_getshdrnum (elf, &shnums) >= 0);

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr != NULL)
	{
	  /* Print the names of symbols referred to by relocations.  */
	  if (shdr->sh_type == SHT_REL)
	    {
	      size_t sh_entsize = gelf_fsize (elf, ELF_T_REL, 1, EV_CURRENT);
	      print_reloc_symnames (elf, scn, shdr, sh_entsize);
	    }
	  else if (shdr->sh_type == SHT_RELA)
	    {
	      size_t sh_entsize = gelf_fsize (elf, ELF_T_RELA, 1, EV_CURRENT);
	      print_reloc_symnames (elf, scn, shdr, sh_entsize);
	    }
	}
    }

  elf_end (elf);
  close (fd);
  return 0;
}
