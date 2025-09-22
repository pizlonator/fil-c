/* Create new section in output file.
   Copyright (C) 2002-2011, 2016 Red Hat, Inc.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libasmP.h>
#include <libelf.h>
#include <system.h>


/* Memory for the default pattern.  The type uses a flexible array
   which does work well with a static initializer.  Work around this by
   wrapping it in a union, whose second member is a char array 1 byte larger
   than struct FillPattern.  According to 6.7.9, this does what we need:

        If an object that has static or thread storage duration is not
        initialized explicitly, then ... if it is a union, the first named
        member is initialized (recursively) according to these rules, and
        any padding is initialized to zero bits.  */

static const union
{
  struct FillPattern pattern;
  char zeroes[sizeof(struct FillPattern) + 1];
} xdefault_pattern =
  {
    .pattern =
    {
      .len = 1
    },
  };
const struct FillPattern *__libasm_default_pattern = &xdefault_pattern.pattern;


static AsmScn_t *
text_newscn (AsmScn_t *result, GElf_Word type, GElf_Xword flags)
{
  /* Buffer where we construct the flag string.  */
  char flagstr[sizeof (GElf_Xword) * 8 + 5];
  char *wp = flagstr;
  const char *typestr = "";

  /* Only write out the flag string if this is the first time the
     section is selected.  Some assemblers cannot cope with the
     .section pseudo-op otherwise.  */
  wp = stpcpy (wp, ", \"");

  if (flags & SHF_WRITE)
    *wp++ = 'w';
  if (flags & SHF_ALLOC)
    *wp++ = 'a';
  if (flags & SHF_EXECINSTR)
    *wp++ = 'x';
  if (flags & SHF_MERGE)
    *wp++ = 'M';
  if (flags & SHF_STRINGS)
    *wp++ = 'S';
  if (flags & SHF_LINK_ORDER)
    *wp++ = 'L';

  *wp++ = '"';

  if (type == SHT_PROGBITS)
    typestr = ",@progbits";
  else if (type == SHT_NOBITS)
    typestr = ",@nobits";

  /* Terminate the string.  */
  *wp = '\0';

  fprintf (result->ctx->out.file, "\t.section \"%s\"%s%s\n",
	   result->name, flagstr, typestr);

  return result;
}


static AsmScn_t *
binary_newscn (AsmScn_t *result, GElf_Word type, GElf_Xword flags,
	       size_t scnname_len)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf_Scn *scn;

  /* The initial subsection has the number zero.  */
  result->subsection_id = 0;

  /* We start at offset zero.  */
  result->offset = 0;
  /* And generic alignment.  */
  result->max_align = 1;

  /* No output yet.  */
  result->content = NULL;

  /* Put the default fill pattern in place.  */
  result->pattern = (struct FillPattern *) __libasm_default_pattern;

  /* There are no subsections so far.  */
  result->subnext = NULL;

  /* Add the name to the section header string table.  */
  result->data.main.strent = dwelf_strtab_add_len (result->ctx->section_strtab,
						   result->name, scnname_len);
  assert (result->data.main.strent != NULL);

  /* Create the new ELF section.  */
  result->data.main.scn = scn = elf_newscn (result->ctx->out.elf);
  if (scn == NULL)
    {
      free (result);
      __libasm_seterrno (ASM_E_LIBELF);
      return NULL;
    }

  /* Not part of a section group (yet).  */
  result->data.main.next_in_group = NULL;

  /* Remember the flags.  */
  shdr = gelf_getshdr (scn, &shdr_mem);

  shdr->sh_flags = flags;
  result->type = shdr->sh_type = type;

  (void) gelf_update_shdr (scn, shdr);

  return result;
}


AsmScn_t *
asm_newscn (AsmCtx_t *ctx, const char *scnname, GElf_Word type,
	    GElf_Xword flags)
{
  size_t scnname_len = strlen (scnname) + 1;
  AsmScn_t *result;

  /* If no context is given there might be an earlier error.  */
  if (ctx == NULL)
    return NULL;

  /* Check whether only flags are set which areselectable by the user.  */
  if (unlikely ((flags & ~(SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR | SHF_MERGE
			   | SHF_STRINGS | SHF_LINK_ORDER)) != 0)
      /* We allow only two section types: data and data without file
	 representation.  */
      || (type != SHT_PROGBITS && unlikely (type != SHT_NOBITS)))
    {
      __libasm_seterrno (ASM_E_INVALID);
      return NULL;
    }

  rwlock_wrlock (ctx->lock);

  /* This is a new section.  */
  result = malloc (sizeof (AsmScn_t) + scnname_len);
  if (result != NULL)
    {
      /* Add the name.  */
      memcpy (result->name, scnname, scnname_len);

      /* Add the reference to the context.  */
      result->ctx = ctx;

      /* Perform operations according to output mode.  */
      result = (unlikely (ctx->textp)
		? text_newscn (result, type, flags)
		: binary_newscn (result, type, flags, scnname_len));

      /* If everything went well finally add the new section to the hash
	 table.  */
      if (result != NULL)
	{
	  result->allnext = ctx->section_list;
	  ctx->section_list = result;
	}
    }

  rwlock_unlock (ctx->lock);

  return result;
}
INTDEF(asm_newscn)
