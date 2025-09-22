/* Copyright (C) 2002, 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include ELFUTILS_HEADER(asm)
#include ELFUTILS_HEADER(ebl)
#include <libelf.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


static const char fname[] = "asm-tst3-out.o";


static const char *scnnames[5] =
  {
    [0] = "",
    [1] = ".data",
    [2] = ".strtab",
    [3] = ".symtab",
    [4] = ".shstrtab"
  };


static unsigned int scntypes[5] =
  {
    [0] = SHT_NULL,
    [1] = SHT_PROGBITS,
    [2] = SHT_STRTAB,
    [3] = SHT_SYMTAB,
    [4] = SHT_STRTAB
  };


int
main (void)
{
  AsmCtx_t *ctx;
  AsmScn_t *scn1;
  AsmScn_t *scn2;
  int result = 0;
  int fd;
  Elf *elf;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  size_t cnt;

  elf_version (EV_CURRENT);

  Ebl *ebl = ebl_openbackend_machine (EM_386);
  if (ebl == NULL)
    {
      puts ("cannot open backend library");
      return 1;
    }

  ctx = asm_begin (fname, ebl, false);
  if (ctx == NULL)
    {
      printf ("cannot create assembler context: %s\n", asm_errmsg (-1));
      return 1;
    }

  /* Create two sections.  */
  scn1 = asm_newscn (ctx, ".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
  scn2 = asm_newsubscn (scn1, 1);
  if (scn1 == NULL || scn2 == NULL)
    {
      printf ("cannot create section in output file: %s\n", asm_errmsg (-1));
      asm_abort (ctx);
      return 1;
    }

  /* Special alignment for the .text section.  */
  if (asm_align (scn1, 16) != 0)
    {
      printf ("cannot align .text section: %s\n", asm_errmsg (-1));
      result = 1;
    }

  /* Add a few strings with names.  */
  if (asm_newsym (scn1, "one", 4, STT_OBJECT, STB_GLOBAL) == NULL)
    {
      printf ("cannot create first name: %s\n", asm_errmsg (-1));
      result = 1;
    }
  if (asm_addstrz (scn1, "one", 4) != 0)
    {
      printf ("cannot insert first string: %s\n", asm_errmsg (-1));
      result = 1;
    }
  if (asm_newsym (scn2, "three", 6, STT_OBJECT, STB_WEAK) == NULL)
    {
      printf ("cannot create second name: %s\n", asm_errmsg (-1));
      result = 1;
    }
  if (asm_addstrz (scn2, "three", 0) != 0)
    {
      printf ("cannot insert second string: %s\n", asm_errmsg (-1));
      result = 1;
    }
  if (asm_newsym (scn1, "two", 4, STT_OBJECT, STB_LOCAL) == NULL)
    {
      printf ("cannot create third name: %s\n", asm_errmsg (-1));
      result = 1;
    }
  if (asm_addstrz (scn1, "two", 4) != 0)
    {
      printf ("cannot insert third string: %s\n", asm_errmsg (-1));
      result = 1;
    }

  /* Create the output file.  */
  if (asm_end (ctx) != 0)
    {
      printf ("cannot create output file: %s\n", asm_errmsg (-1));
      asm_abort (ctx);
      return 1;
    }

  /* Check the file.  */
  fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      printf ("cannot open generated file: %m\n");
      result = 1;
      goto out;
    }

  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      printf ("cannot create ELF descriptor: %s\n", elf_errmsg (-1));
      result = 1;
      goto out_close;
    }
  if (elf_kind (elf) != ELF_K_ELF)
    {
      puts ("not a valid ELF file");
      result = 1;
      goto out_close2;
    }

  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      printf ("cannot get ELF header: %s\n", elf_errmsg (-1));
      result = 1;
      goto out_close2;
    }

  for (cnt = 1; cnt < 5; ++cnt)
    {
      Elf_Scn *scn;
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr;

      scn = elf_getscn (elf, cnt);
      if (scn == NULL)
	{
	  printf ("cannot get section %zd: %s\n", cnt, elf_errmsg (-1));
	  result = 1;
	  continue;
	}

      shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	{
	  printf ("cannot get section header for section %zd: %s\n",
		  cnt, elf_errmsg (-1));
	  result = 1;
	  continue;
	}

      if (strcmp (elf_strptr (elf, ehdr->e_shstrndx, shdr->sh_name),
		  scnnames[cnt]) != 0)
	{
	  printf ("section %zd's name differs: %s vs %s\n", cnt,
		  elf_strptr (elf, ehdr->e_shstrndx, shdr->sh_name),
		  scnnames[cnt]);
	  result = 1;
	}

      if (shdr->sh_type != scntypes[cnt])
	{
	  printf ("section %zd's type differs\n", cnt);
	  result = 1;
	}

      if ((cnt == 1 && shdr->sh_flags != (SHF_ALLOC | SHF_WRITE))
	  || (cnt != 1 && shdr->sh_flags != 0))
	{
	  printf ("section %zd's flags differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_addr != 0)
	{
	  printf ("section %zd's address differs\n", cnt);
	  result = 1;
	}

      if (cnt == 3)
	{
	  Elf_Data *data;

	  if (shdr->sh_link != 2)
	    {
	      puts ("symbol table has incorrect link");
	      result = 1;
	    }

	  data = elf_getdata (scn, NULL);
	  if (data == NULL)
	    {
	      puts ("cannot get data of symbol table");
	      result = 1;
	    }
	  else
	    {
	      size_t inner;

	      for (inner = 1;
		   inner < (shdr->sh_size
			    / gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT));
		   ++inner)
		{
		  GElf_Sym sym_mem;
		  GElf_Sym *sym;

		  sym = gelf_getsym (data, inner, &sym_mem);
		  if (sym == NULL)
		    {
		      printf ("cannot get symbol %zu: %s\n",
			      inner, elf_errmsg (-1));
		      result = 1;
		    }
		  else
		    {
		      /* The order of the third and fourth entry depends
			 on how the hash table is organized.  */
		      static const char *names[4] =
			{
			  [0] = "",
			  [1] = "two",
			  [2] = "one",
			  [3] = "three"
			};
		      static const int info[4] =
			{
			  [0] = GELF_ST_INFO (STB_LOCAL, STT_NOTYPE),
			  [1] = GELF_ST_INFO (STB_LOCAL, STT_OBJECT),
			  [2] = GELF_ST_INFO (STB_GLOBAL, STT_OBJECT),
			  [3] = GELF_ST_INFO (STB_WEAK, STT_OBJECT)
			};
		      static const unsigned value[4] =
			{
			  [0] = 0,
			  [1] = 4,
			  [2] = 0,
			  [3] = 8
			};

		      if (strcmp (names[inner],
				  elf_strptr (elf, shdr->sh_link,
					      sym->st_name)) != 0)
			{
			  printf ("symbol %zu has different name\n", inner);
			  result = 1;
			}

		      if (sym->st_value != value[inner])
			{
			  printf ("symbol %zu has wrong value\n", inner);
			  result = 1;
			}

		      if (sym->st_other != 0)
			{
			  printf ("symbol %zu has wrong other info\n", inner);
			  result = 1;
			}

		      if (sym->st_shndx != 1)
			{
			  printf ("symbol %zu has wrong section reference\n",
				  inner);
			  result = 1;
			}

		      if (sym->st_info != info[inner])
			{
			  printf ("symbol %zu has wrong type or binding\n",
				  inner);
			  result = 1;
			}

		      if ((inner != 3 && sym->st_size != 4)
			  || (inner == 3 && sym->st_size != 6))
			{
			  printf ("symbol %zu has wrong size\n", inner);
			  result = 1;
			}
		    }
		}
	    }
	}
    }

 out_close2:
  elf_end (elf);
 out_close:
  close (fd);
 out:
  /* We don't need the file anymore.  */
  unlink (fname);

  ebl_closebackend (ebl);

  return result;
}
