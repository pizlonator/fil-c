/* Define new symbol for current position in given section.
   Copyright (C) 2002, 2005, 2016, 2017 Red Hat, Inc.
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


AsmSym_t *
asm_newsym (AsmScn_t *asmscn, const char *name, GElf_Xword size,
	    int type, int binding)
{
/* We don't really expect labels with many digits, but in theory it could
   be 10 digits (plus ".L" and a zero terminator).  */
#define TEMPSYMLEN 13
  char tempsym[TEMPSYMLEN];
  AsmSym_t *result;

  if (asmscn == NULL)
    /* Something went wrong before.  */
    return NULL;

  /* Generate a temporary symbol if necessary.  */
  if (name == NULL)
    {
      /* If a local symbol name is created the symbol better have
	 local binding.  */
      if (binding != STB_LOCAL)
	{
	  __libasm_seterrno (ASM_E_INVALID);
	  return NULL;
	}

      // XXX This requires getting the format from the machine backend.  */
      snprintf (tempsym, TEMPSYMLEN, ".L%07u", asmscn->ctx->tempsym_count++);

      name = tempsym;
    }

  size_t name_len = strlen (name) + 1;

  result = malloc (sizeof (AsmSym_t) + name_len);
  if (result == NULL)
    return NULL;

  rwlock_wrlock (asmscn->ctx->lock);

  result->scn = asmscn;
  result->offset = asmscn->offset;
  result->size = size;
  result->type = type;
  result->binding = binding;
  result->symidx = 0;
  result->strent = dwelf_strtab_add (asmscn->ctx->symbol_strtab,
				     memcpy (result + 1, name, name_len));

  if (unlikely (asmscn->ctx->textp))
    {
      /* We are only interested in the name and don't need to know whether
	 it is a local name or not.  */
      /* First print the binding pseudo-op.  */
      if (binding == STB_GLOBAL)
	fprintf (asmscn->ctx->out.file, "\t.globl\t%s\n", name);
      else if (binding == STB_WEAK)
	fprintf (asmscn->ctx->out.file, "\t.weak\t%s\n", name);

      /* Next the symbol type.  */
      if (type == STT_OBJECT)
	fprintf (asmscn->ctx->out.file, "\t.type\t%s,@object\n", name);
      else if (type == STT_FUNC)
	fprintf (asmscn->ctx->out.file, "\t.type\t%s,@function\n", name);

      /* Finally the size and the label.  */
      fprintf (asmscn->ctx->out.file, "\t.size\t%s,%" PRIuMAX "\n%s:\n",
	       name, (uintmax_t) size, name);
    }
  else
    {
      /* Put the symbol in the hash table so that we can later find it.  */
      if (asm_symbol_tab_insert (&asmscn->ctx->symbol_tab, elf_hash (name),
				 result) != 0)
	{
	  /* The symbol already exists.  */
	  __libasm_seterrno (ASM_E_DUPLSYM);
	  /* Note that we can free the entry since there must be no
	     reference in the string table to the string.  We can only
	     fail to insert the symbol into the symbol table if there
	     is already a symbol with this name.  In this case the
	     dwelf_strtab_add function would use the previously provided
	     name.  */
	  free (result);
	  result = NULL;
	}
      else if (name != tempsym && asm_emit_symbol_p (name))
	/* Only count non-private symbols.  */
	++asmscn->ctx->nsymbol_tab;
    }

  rwlock_unlock (asmscn->ctx->lock);

  return result;
}
