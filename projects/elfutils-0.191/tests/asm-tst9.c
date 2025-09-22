/* Copyright (C) 2002-2010 Red Hat, Inc.
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
#include <string.h>
#include <unistd.h>


static const char fname[] = "asm-tst9-out.o";


static int32_t input[] =
  {
    0, 1, 129, 510, 2000, 33000, 0x7ffffff, 0x7fffffff
  };
#define ninput (sizeof (input) / sizeof (input[0]))


static const GElf_Ehdr expected_ehdr =
  {
    .e_ident = { [EI_MAG0] = ELFMAG0,
		 [EI_MAG1] = ELFMAG1,
		 [EI_MAG2] = ELFMAG2,
		 [EI_MAG3] = ELFMAG3,
		 [EI_CLASS] = ELFCLASS32,
		 [EI_DATA] = ELFDATA2LSB,
		 [EI_VERSION] = EV_CURRENT },
    .e_type = ET_REL,
    .e_machine = EM_386,
    .e_version = EV_CURRENT,
    .e_shoff = 180,
    .e_ehsize = sizeof (Elf32_Ehdr),
    .e_shentsize = sizeof (Elf32_Shdr),
    .e_shnum = 3,
    .e_shstrndx = 2
  };


static const char *scnnames[3] =
  {
    [0] = "",
    [1] = ".data",
    [2] = ".shstrtab"
  };


static const char expecteddata[] =
  {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x7f,
    0x81, 0x01, 0x81, 0x01, 0xff, 0xfe, 0xff, 0xff, 0x0f, 0xff, 0x7e, 0xfe,
    0x03, 0xfe, 0x03, 0x82, 0xfc, 0xff, 0xff, 0x0f, 0x82, 0x7c, 0xd0, 0x0f,
    0xd0, 0x0f, 0xb0, 0xf0, 0xff, 0xff, 0x0f, 0xb0, 0x70, 0xe8, 0x81, 0x02,
    0xe8, 0x81, 0x02, 0x98, 0xfe, 0xfd, 0xff, 0x0f, 0x98, 0xfe, 0x7d, 0xff,
    0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0x3f, 0x81, 0x80, 0x80, 0xc0, 0x0f,
    0x81, 0x80, 0x80, 0x40, 0xff, 0xff, 0xff, 0xff, 0x07, 0xff, 0xff, 0xff,
    0xff, 0x07, 0x81, 0x80, 0x80, 0x80, 0x08, 0x81, 0x80, 0x80, 0x80, 0x78
  };


int
main (void)
{
  AsmCtx_t *ctx;
  AsmScn_t *scn;
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
  scn = asm_newscn (ctx, ".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
  if (scn == NULL)
    {
      printf ("cannot create section in output file: %s\n", asm_errmsg (-1));
      asm_abort (ctx);
      return 1;
    }

  /* Special alignment for the .text section.  */
  if (asm_align (scn, 16) != 0)
    {
      printf ("cannot align .text section: %s\n", asm_errmsg (-1));
      result = 1;
    }

  /* Add a few ULEB128 and SLEB128 numbers.  */
  for (cnt = 0; cnt < ninput; ++cnt)
    {
      if (asm_adduleb128 (scn, input[cnt]) != 0)
	{
	  printf ("cannot insert uleb %" PRIu32 ": %s\n",
		  (uint32_t) input[cnt], asm_errmsg (-1));
	  result = 1;
	}

      if (asm_addsleb128 (scn, input[cnt]) != 0)
	{
	  printf ("cannot insert sleb %" PRId32 ": %s\n",
		  input[cnt], asm_errmsg (-1));
	  result = 1;
	}

      if (asm_adduleb128 (scn, -input[cnt]) != 0)
	{
	  printf ("cannot insert uleb %" PRIu32 ": %s\n",
		  (uint32_t) -input[cnt], asm_errmsg (-1));
	  result = 1;
	}

      if (asm_addsleb128 (scn, -input[cnt]) != 0)
	{
	  printf ("cannot insert sleb %" PRId32 ": %s\n",
		  -input[cnt], asm_errmsg (-1));
	  result = 1;
	}
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

  if (memcmp (ehdr, &expected_ehdr, sizeof (GElf_Ehdr)) != 0)
    {
      puts ("ELF header does not match");
      result = 1;
      goto out_close2;
    }

  for (cnt = 1; cnt < 3; ++cnt)
    {
      Elf_Scn *escn;
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr;

      escn = elf_getscn (elf, cnt);
      if (escn == NULL)
	{
	  printf ("cannot get section %zd: %s\n", cnt, elf_errmsg (-1));
	  result = 1;
	  continue;
	}

      shdr = gelf_getshdr (escn, &shdr_mem);
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

      if (shdr->sh_type != (cnt == 2 ? SHT_STRTAB : SHT_PROGBITS))
	{
	  printf ("section %zd's type differs\n", cnt);
	  result = 1;
	}

      if ((cnt == 1 && shdr->sh_flags != (SHF_ALLOC | SHF_WRITE))
	  || (cnt == 2 && shdr->sh_flags != 0))
	{
	  printf ("section %zd's flags differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_addr != 0)
	{
	  printf ("section %zd's address differs\n", cnt);
	  result = 1;
	}

      if ((cnt == 1 && shdr->sh_offset != ((sizeof (Elf32_Ehdr) + 15) & ~15))
	  || (cnt == 2
	      && shdr->sh_offset != (((sizeof (Elf32_Ehdr) + 15) & ~15)
				     + sizeof (expecteddata))))
	{
	  printf ("section %zd's offset differs\n", cnt);
	  result = 1;
	}

      if ((cnt == 1 && shdr->sh_size != sizeof (expecteddata))
	  || (cnt == 2 && shdr->sh_size != 17))
	{
	  printf ("section %zd's size differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_link != 0)
	{
	  printf ("section %zd's link differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_info != 0)
	{
	  printf ("section %zd's info differs\n", cnt);
	  result = 1;
	}

      if ((cnt == 1 && shdr->sh_addralign != 16)
	  || (cnt != 1 && shdr->sh_addralign != 1))
	{
	  printf ("section %zd's addralign differs\n", cnt);
	  result = 1;
	}

      if (shdr->sh_entsize != 0)
	{
	  printf ("section %zd's entsize differs\n", cnt);
	  result = 1;
	}

      if (cnt == 1)
	{
	  Elf_Data *data = elf_getdata (escn, NULL);

	  if (data == NULL)
	    {
	      printf ("cannot get data of section %zd\n", cnt);
	      result = 1;
	    }
	  else
	    {
	      if (data->d_size != sizeof (expecteddata))
		{
		  printf ("data block size of section %zd wrong: got %zd, "
			  "expected 96\n", cnt, data->d_size);
		  result = 1;
		}

	      if (memcmp (data->d_buf, expecteddata, sizeof (expecteddata))
		  != 0)
		{
		  printf ("data block content of section %zd wrong\n", cnt);
		  result = 1;
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
