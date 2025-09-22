/* Create context descriptor for disassembler.
   Copyright (C) 2005, 2008 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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

#include "libasmP.h"
#include "libeblP.h"


DisasmCtx_t *
disasm_begin (Ebl *ebl, Elf *elf, DisasmGetSymCB_t symcb)
{
  if (ebl == NULL)
    return NULL;

  if (ebl->disasm == NULL)
    {
      __libasm_seterrno (ASM_E_ENOSUP);
      return NULL;
    }

  DisasmCtx_t *ctx = malloc (sizeof (DisasmCtx_t));
  if (ctx == NULL)
    {
      __libasm_seterrno (ASM_E_NOMEM);
      return NULL;
    }

  ctx->ebl = ebl;
  ctx->elf = elf;
  ctx->symcb = symcb;

  return ctx;
}
