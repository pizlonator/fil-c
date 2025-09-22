/* Abort operations on the assembler context, free all resources.
   Copyright (C) 2002, 2005 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>

#include <libasmP.h>
#include <libelf.h>


int
asm_abort (AsmCtx_t *ctx)
{
  if (ctx == NULL)
    /* Something went wrong earlier.  */
    return -1;

  if (likely (! ctx->textp))
    /* First free the ELF file.  We don't care about the result.  */
    (void) elf_end (ctx->out.elf);

  /* Now close the temporary file and remove it.  */
  if (ctx->fd != -1)
    (void) unlink (ctx->tmp_fname);

  /* Free the resources.  */
  __libasm_finictx (ctx);

  return 0;
}
