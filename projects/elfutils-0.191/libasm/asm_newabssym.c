/* Create new ABS symbol.
   Copyright (C) 2002, 2016 Red Hat, Inc.
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libasmP.h>
#include <system.h>


/* Object for special COMMON section.  */
static const AsmScn_t __libasm_abs_scn =
  {
    .data = {
      .main = {
	.scn = ASM_ABS_SCN
      }
    }
  };


AsmSym_t *
asm_newabssym (AsmCtx_t *ctx, const char *name, GElf_Xword size,
	       GElf_Addr value, int type, int binding)
{
  AsmSym_t *result;

  if (ctx == NULL)
    /* Something went wrong before.  */
    return NULL;

  /* Common symbols are public.  Therefore the user must provide a
     name.  */
  if (name == NULL)
    {
      __libasm_seterrno (ASM_E_INVALID);
      return NULL;
    }

  rwlock_wrlock (ctx->lock);

  result = malloc (sizeof (AsmSym_t));
  if (result == NULL)
    return NULL;

  result->scn = (AsmScn_t *) &__libasm_abs_scn;
  result->size = size;
  result->type = type;
  result->binding = binding;
  result->symidx = 0;
  result->strent = dwelf_strtab_add (ctx->symbol_strtab, name);

  /* The value of an ABS symbol must not be modified.  Since there are
     no subsection and the initial offset of the section is 0 we can
     get the alignment recorded by storing it into the offset
     field.  */
  result->offset = value;

  if (unlikely (ctx->textp))
    {
      /* An absolute symbol can be defined by giving a symbol a
	 specific value.  */
      if (binding == STB_GLOBAL)
	fprintf (ctx->out.file, "\t.globl %s\n", name);
      else if (binding == STB_WEAK)
	fprintf (ctx->out.file, "\t.weak %s\n", name);

      if (type == STT_OBJECT)
	fprintf (ctx->out.file, "\t.type %s,@object\n", name);
      else if (type == STT_FUNC)
	fprintf (ctx->out.file, "\t.type %s,@function\n", name);

      fprintf (ctx->out.file, "%s = %llu\n",
	       name, (unsigned long long int) value);

      if (size != 0)
	fprintf (ctx->out.file, "\t.size %s, %llu\n",
		 name, (unsigned long long int) size);
    }
  else
    {
      /* Put the symbol in the hash table so that we can later find it.  */
      if (asm_symbol_tab_insert (&ctx->symbol_tab, elf_hash (name), result)
	  != 0)
	{
	  /* The symbol already exists.  */
	  __libasm_seterrno (ASM_E_DUPLSYM);
	  free (result);
	  result = NULL;
	}
      else if (name != NULL && asm_emit_symbol_p (name))
	/* Only count non-private symbols.  */
	++ctx->nsymbol_tab;
    }

  rwlock_unlock (ctx->lock);

  return result;
}
