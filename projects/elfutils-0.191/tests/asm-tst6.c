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

#include ELFUTILS_HEADER(asm)
#include ELFUTILS_HEADER(ebl)
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include <system.h>


static const char fname[] = "asm-tst6-out.o";


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

  for (cnt = 0; cnt < 22000; ++cnt)
    {
      char buf[512];
      AsmScnGrp_t *grp;
      AsmScn_t *scn;
      AsmSym_t *sym;

      snprintf (buf, sizeof (buf), ".grp%zu", cnt);
      grp = asm_newscngrp (ctx, buf, NULL, 0);
      if (grp == NULL)
	{
	  printf ("cannot section group %zu: %s\n", cnt, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      scn = asm_newscn_ingrp (ctx, ".data", SHT_PROGBITS,
			      SHF_ALLOC | SHF_WRITE, grp);
      if (scn == NULL)
	{
	  printf ("cannot data section for group %zu: %s\n",
		  cnt, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      /* Add a name.  */
      snprintf (buf, sizeof (buf), "%zu", cnt);
      sym = asm_newsym (scn, buf, sizeof (uint32_t), STT_OBJECT,
			STB_GLOBAL);
      if (sym == NULL)
	{
	  printf ("cannot create symbol \"%s\": %s\n", buf, asm_errmsg (-1));
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

      /* Now we have a symbol, use it as the signature.  */
      if (asm_scngrp_newsignature (grp, sym) != 0)
	{
	  printf ("cannot set signature for section group %zu: %s\n",
		  cnt, asm_errmsg (-1));
	  asm_abort (ctx);
	  return 1;
	}

      /* Create a phony debug info section.  */
      scn = asm_newscn_ingrp (ctx, ".stab", SHT_PROGBITS, 0, grp);
      if (scn == NULL)
	{
	  printf ("cannot stab section for group %zu: %s\n",
		  cnt, asm_errmsg (-1));
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
    result = WEXITSTATUS (system ("../src/elflint -q asm-tst6-out.o"));

  /* We don't need the file anymore.  */
  unlink (fname);

  ebl_closebackend (ebl);

  return result;
}
