/* Copyright (C) 2002-2012 Red Hat, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


static const char fname[] = "asm-tst4-out.o";


int
main (void)
{
  AsmCtx_t *ctx;
  int result = 0;
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

  /* Create 66000 sections.  */
  for (cnt = 0; cnt < 66000; ++cnt)
    {
      char buf[20];
      AsmScn_t *scn;

      /* Create a unique name.  */
      snprintf (buf, sizeof (buf), ".data.%zu", cnt);

      /* Create the section.  */
      scn = asm_newscn (ctx, buf, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
      if (scn == NULL)
	{
	  printf ("cannot create section \"%s\" in output file: %s\n",
		  buf, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      /* Add some content.  */
      if (asm_adduint32 (scn, cnt) != 0)
	{
	  printf ("cannot create content of section \"%s\": %s\n",
		  buf, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}
    }

  /* Create the output file.  */
  if (asm_end (ctx) != 0)
    {
      printf ("cannot create output file: %s\n", asm_errmsg (-1));
      asm_abort (ctx);
      return 1;
    }

  if (result == 0)
    result = WEXITSTATUS (system ("../src/elflint -q asm-tst4-out.o"));

  /* We don't need the file anymore.  */
  unlink (fname);

  ebl_closebackend (ebl);

  return result;
}
