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
#include <inttypes.h>
#include ELFUTILS_HEADER(asm)
#include ELFUTILS_HEADER(ebl)
#include <libelf.h>
#include <stdio.h>
#include <unistd.h>


static const char fname[] = "asm-tst8-out.o";


int
main (void)
{
  int result = 0;
  size_t cnt;
  AsmCtx_t *ctx;
  Elf *elf;
  int fd;

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

  if (asm_newabssym (ctx, "tst8-out.s", 4, 0xfeedbeef, STT_FILE, STB_LOCAL)
      == NULL)
    {
      printf ("cannot create absolute symbol: %s\n", asm_errmsg (-1));
      asm_abort (ctx);
      return 1;
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

  for (cnt = 1; 1; ++cnt)
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
      /* We are looking for the symbol table.  */
      if (shdr->sh_type != SHT_SYMTAB)
	continue;

      for (cnt = 1; cnt< (shdr->sh_size
			  / gelf_fsize (elf, ELF_T_SYM, 1, EV_CURRENT));
	   ++cnt)
	{
	  GElf_Sym sym_mem;
	  GElf_Sym *sym;

	  if (cnt > 1)
	    {
	      puts ("too many symbol");
	      result = 1;
	      break;
	    }

	  sym = gelf_getsym (elf_getdata (scn, NULL), cnt, &sym_mem);
	  if (sym == NULL)
	    {
	      printf ("cannot get symbol %zu: %s\n", cnt, elf_errmsg (-1));
	      result = 1;
	    }
	  else
	    {
	      if (sym->st_shndx != SHN_ABS)
		{
		  printf ("expected common symbol, got section %u\n",
			  (unsigned int) sym->st_shndx);
		  result = 1;
		}

	      if (sym->st_value != 0xfeedbeef)
		{
		  printf ("requested value 0xfeedbeef, is %#" PRIxMAX "\n",
			  (uintmax_t) sym->st_value);
		  result = 1;
		}

	      if (sym->st_size != 4)
		{
		  printf ("requested size 4, is %" PRIuMAX "\n",
			  (uintmax_t) sym->st_value);
		  result = 1;
		}

	      if (GELF_ST_TYPE (sym->st_info) != STT_FILE)
		{
		  printf ("requested type FILE, is %u\n",
			  (unsigned int) GELF_ST_TYPE (sym->st_info));
		  result = 1;
		}
	    }
	}

      break;
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
