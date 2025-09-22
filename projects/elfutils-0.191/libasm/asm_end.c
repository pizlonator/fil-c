/* Finalize operations on the assembler context, free all resources.
   Copyright (C) 2002, 2003, 2005, 2016 Red Hat, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libasmP.h>
#include <libelf.h>


static int
text_end (AsmCtx_t *ctx __attribute__ ((unused)))
{
  if (fflush (ctx->out.file) != 0)
    {
      __libasm_seterrno (ASM_E_IOERROR);
      return -1;
    }

  return 0;
}


static int
binary_end (AsmCtx_t *ctx)
{
  void *symtab = NULL;
  Dwelf_Strent *symscn_strent = NULL;
  Dwelf_Strent *strscn_strent = NULL;
  Dwelf_Strent *xndxscn_strent = NULL;
  Elf_Scn *shstrscn;
  Dwelf_Strent *shstrscn_strent;
  size_t shstrscnndx;
  size_t symscnndx = 0;
  size_t strscnndx = 0;
  size_t xndxscnndx = 0;
  Elf_Data *data;
  Elf_Data *shstrtabdata;
  Elf_Data *strtabdata = NULL;
  Elf_Data *xndxdata = NULL;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  AsmScn_t *asmscn;
  int result = 0;

  /* Iterate over the created sections and compute the offsets of the
     various subsections and fill in the content.  */
  for (asmscn = ctx->section_list; asmscn != NULL; asmscn = asmscn->allnext)
    {
#if 0
      Elf_Scn *scn = elf_getscn (ctx->out.elf, asmscn->data.main.scnndx);
#else
      Elf_Scn *scn = asmscn->data.main.scn;
#endif
      off_t offset = 0;
      AsmScn_t *asmsubscn = asmscn;

      do
	{
	  struct AsmData *content = asmsubscn->content;
	  bool first = true;

	  offset = ((offset + asmsubscn->max_align - 1)
		    & ~(asmsubscn->max_align - 1));

	  /* Update the offset for this subsection.  This field now
	     stores the offset of the first by in this subsection.  */
	  asmsubscn->offset = offset;

	  /* Note that the content list is circular.  */
	  if (content != NULL)
	    do
	      {
		Elf_Data *newdata = elf_newdata (scn);

		if (newdata == NULL)
		  {
		    __libasm_seterrno (ASM_E_LIBELF);
		    return -1;
		  }

		newdata->d_buf = content->data;
		newdata->d_type = ELF_T_BYTE;
		newdata->d_size = content->len;
		newdata->d_off = offset;
		newdata->d_align = first ? asmsubscn->max_align : 1;

		offset += content->len;
	      }
	    while ((content = content->next) != asmsubscn->content);
	}
      while ((asmsubscn = asmsubscn->subnext) != NULL);
    }


  /* Create the symbol table if necessary.  */
  if (ctx->nsymbol_tab > 0)
    {
      /* Create the symbol table and string table section names.  */
      symscn_strent = dwelf_strtab_add_len (ctx->section_strtab, ".symtab", 8);
      strscn_strent = dwelf_strtab_add_len (ctx->section_strtab, ".strtab", 8);

      /* Create the symbol string table section.  */
      Elf_Scn *strscn = elf_newscn (ctx->out.elf);
      strtabdata = elf_newdata (strscn);
      shdr = gelf_getshdr (strscn, &shdr_mem);
      if (strtabdata == NULL || shdr == NULL)
	{
	  __libasm_seterrno (ASM_E_LIBELF);
	  return -1;
	}
      strscnndx = elf_ndxscn (strscn);

      dwelf_strtab_finalize (ctx->symbol_strtab, strtabdata);

      shdr->sh_type = SHT_STRTAB;
      assert (shdr->sh_entsize == 0);

      (void) gelf_update_shdr (strscn, shdr);

      /* Create the symbol table section.  */
      Elf_Scn *symscn = elf_newscn (ctx->out.elf);
      data = elf_newdata (symscn);
      shdr = gelf_getshdr (symscn, &shdr_mem);
      if (data == NULL || shdr == NULL)
	{
	  __libasm_seterrno (ASM_E_LIBELF);
	  return -1;
	}
      symscnndx = elf_ndxscn (symscn);

      /* We know how many symbols there will be in the symbol table.  */
      data->d_size = gelf_fsize (ctx->out.elf, ELF_T_SYM,
				 ctx->nsymbol_tab + 1, EV_CURRENT);
      symtab = malloc (data->d_size);
      if (symtab == NULL)
	return -1;
      data->d_buf = symtab;
      data->d_type = ELF_T_SYM;
      data->d_off = 0;

      /* Clear the first entry.  */
      GElf_Sym syment;
      memset (&syment, '\0', sizeof (syment));
      (void) gelf_update_sym (data, 0, &syment);

      /* Iterate over the symbol table.  */
      void *runp = NULL;
      int ptr_local = 1;	/* Start with index 1; zero remains unused.  */
      int ptr_nonlocal = ctx->nsymbol_tab;
      uint32_t *xshndx = NULL;
      AsmSym_t *sym;
      while ((sym = asm_symbol_tab_iterate (&ctx->symbol_tab, &runp)) != NULL)
	if (asm_emit_symbol_p (dwelf_strent_str (sym->strent)))
	  {
	    assert (ptr_local <= ptr_nonlocal);

	    syment.st_name = dwelf_strent_off (sym->strent);
	    syment.st_info = GELF_ST_INFO (sym->binding, sym->type);
	    syment.st_other = 0;
	    syment.st_value = sym->scn->offset + sym->offset;
	    syment.st_size = sym->size;

	    /* Add local symbols at the beginning, the other from
	       the end.  */
	    int ptr = sym->binding == STB_LOCAL ? ptr_local++ : ptr_nonlocal--;

	    /* Determine the section index.  We have to handle the
	       overflow correctly.  */
	    Elf_Scn *scn = (sym->scn->subsection_id == 0
			    ? sym->scn->data.main.scn
			    : sym->scn->data.up->data.main.scn);

	    Elf32_Word ndx;
	    if (unlikely (scn == ASM_ABS_SCN))
	      ndx = SHN_ABS;
	    else if (unlikely (scn == ASM_COM_SCN))
	      ndx = SHN_COMMON;
	    else if (unlikely ((ndx = elf_ndxscn (scn)) >= SHN_LORESERVE))
	      {
		if (unlikely (xshndx == NULL))
		  {
		    /* The extended section index section does not yet
		       exist.  */
		    Elf_Scn *xndxscn;

		    xndxscn = elf_newscn (ctx->out.elf);
		    xndxdata = elf_newdata (xndxscn);
		    shdr = gelf_getshdr (xndxscn, &shdr_mem);
		    if (xndxdata == NULL || shdr == NULL)
		      {
			__libasm_seterrno (ASM_E_LIBELF);
			return -1;
		      }
		    xndxscnndx = elf_ndxscn (xndxscn);

		    shdr->sh_type = SHT_SYMTAB_SHNDX;
		    shdr->sh_entsize = sizeof (Elf32_Word);
		    shdr->sh_addralign = sizeof (Elf32_Word);
		    shdr->sh_link = symscnndx;

		    (void) gelf_update_shdr (xndxscn, shdr);

		    xndxscn_strent = dwelf_strtab_add_len (ctx->section_strtab,
							   ".symtab_shndx",
							   14);

		    /* Note that using 'elf32_fsize' instead of
		       'gelf_fsize' here is correct.  */
		    xndxdata->d_size = elf32_fsize (ELF_T_WORD,
						    ctx->nsymbol_tab + 1,
						    EV_CURRENT);
		    xshndx = xndxdata->d_buf = calloc (1, xndxdata->d_size);
		    if (xshndx == NULL)
		      return -1;
		    /* Using ELF_T_WORD here relies on the fact that the
		       32- and 64-bit types are the same size.  */
		    xndxdata->d_type = ELF_T_WORD;
		    xndxdata->d_off = 0;
		  }

		/* Store the real section index in the extended section
		   index table.  */
		assert ((size_t) ptr < ctx->nsymbol_tab + 1);
		xshndx[ptr] = ndx;

		/* And signal that this happened.  */
		ndx = SHN_XINDEX;
	      }
	    syment.st_shndx = ndx;

	    /* Remember where we put the symbol.  */
	    sym->symidx = ptr;

	    (void) gelf_update_sym (data, ptr, &syment);
	  }

      assert (ptr_local == ptr_nonlocal + 1);

      shdr->sh_type = SHT_SYMTAB;
      shdr->sh_link = strscnndx;
      shdr->sh_info = ptr_local;
      shdr->sh_entsize = gelf_fsize (ctx->out.elf, ELF_T_SYM, 1, EV_CURRENT);
      shdr->sh_addralign = gelf_fsize (ctx->out.elf, ELF_T_ADDR, 1,
				       EV_CURRENT);

      (void) gelf_update_shdr (symscn, shdr);
    }


  /* Create the section header string table section and fill in the
     references in the section headers.  */
  shstrscn = elf_newscn (ctx->out.elf);
  shstrtabdata = elf_newdata (shstrscn);
  shdr = gelf_getshdr (shstrscn, &shdr_mem);
  if (shstrscn == NULL || shstrtabdata == NULL || shdr == NULL)
    {
      __libasm_seterrno (ASM_E_LIBELF);
      return -1;
    }


  /* Add the name of the section header string table.  */
  shstrscn_strent = dwelf_strtab_add_len (ctx->section_strtab,
					  ".shstrtab", 10);

  dwelf_strtab_finalize (ctx->section_strtab, shstrtabdata);

  shdr->sh_type = SHT_STRTAB;
  assert (shdr->sh_entsize == 0);
  shdr->sh_name = dwelf_strent_off (shstrscn_strent);

  (void) gelf_update_shdr (shstrscn, shdr);


  /* Create the section groups.  */
  if (ctx->groups != NULL)
    {
      AsmScnGrp_t *runp = ctx->groups->next;

      do
	{
	  Elf_Scn *scn;
	  Elf32_Word *grpdata;

	  scn = runp->scn;
	  assert (scn != NULL);
	  shdr = gelf_getshdr (scn, &shdr_mem);
	  assert (shdr != NULL);

	  data = elf_newdata (scn);
	  if (data == NULL)
	    {
	      __libasm_seterrno (ASM_E_LIBELF);
	      return -1;
	    }

	  /* It is correct to use 'elf32_fsize' instead of 'gelf_fsize'
	     here.  */
	  data->d_size = elf32_fsize (ELF_T_WORD, runp->nmembers + 1,
				      EV_CURRENT);
	  grpdata = data->d_buf = malloc (data->d_size);
	  if (grpdata == NULL)
	    return -1;
	  data->d_type = ELF_T_WORD;
	  data->d_off = 0;
	  data->d_align = elf32_fsize (ELF_T_WORD, 1, EV_CURRENT);

	  /* The first word of the section is filled with the flag word.  */
	  *grpdata++ = runp->flags;

	  if (runp->members != NULL)
	    {
	      AsmScn_t *member = runp->members->data.main.next_in_group;

	      do
		{
		  /* Only sections, not subsections, can be registered
		     as member of a group.  The subsections get
		     automatically included.  */
		  assert (member->subsection_id == 0);

		  *grpdata++ = elf_ndxscn (member->data.main.scn);
		}
	      while ((member = member->data.main.next_in_group)
		     != runp->members->data.main.next_in_group);
	    }

	  /* Construct the section header.  */
	  shdr->sh_name = dwelf_strent_off (runp->strent);
	  shdr->sh_type = SHT_GROUP;
	  shdr->sh_flags = 0;
	  shdr->sh_link = symscnndx;
	  /* If the user did not specify a signature we use the initial
	     empty symbol in the symbol table as the signature.  */
	  shdr->sh_info = (runp->signature != NULL
			   ? runp->signature->symidx : 0);

	  (void) gelf_update_shdr (scn, shdr);
	}
      while ((runp = runp->next) != ctx->groups->next);
    }


  /* Add the name to the symbol section.  */
  if (likely (symscnndx != 0))
    {
      Elf_Scn *scn = elf_getscn (ctx->out.elf, symscnndx);

      shdr = gelf_getshdr (scn, &shdr_mem);

      shdr->sh_name = dwelf_strent_off (symscn_strent);

      (void) gelf_update_shdr (scn, shdr);


      /* Add the name to the string section.  */
      assert (strscnndx != 0);
      scn = elf_getscn (ctx->out.elf, strscnndx);

      shdr = gelf_getshdr (scn, &shdr_mem);

      shdr->sh_name = dwelf_strent_off (strscn_strent);

      (void) gelf_update_shdr (scn, shdr);


      /* Add the name to the extended symbol index section.  */
      if (xndxscnndx != 0)
	{
	  scn = elf_getscn (ctx->out.elf, xndxscnndx);

	  shdr = gelf_getshdr (scn, &shdr_mem);

	  shdr->sh_name = dwelf_strent_off (xndxscn_strent);

	  (void) gelf_update_shdr (scn, shdr);
	}
    }


  /* Iterate over the created sections and fill in the names.  */
  for (asmscn = ctx->section_list; asmscn != NULL; asmscn = asmscn->allnext)
    {
      shdr = gelf_getshdr (asmscn->data.main.scn, &shdr_mem);
      /* This better should not fail.  */
      assert (shdr != NULL);

      shdr->sh_name = dwelf_strent_off (asmscn->data.main.strent);

      /* We now know the maximum alignment.  */
      shdr->sh_addralign = asmscn->max_align;

      (void) gelf_update_shdr (asmscn->data.main.scn, shdr);
    }

  /* Put the reference to the section header string table in the ELF
     header.  */
  ehdr = gelf_getehdr (ctx->out.elf, &ehdr_mem);
  assert (ehdr != NULL);

  shstrscnndx = elf_ndxscn (shstrscn);
  if (unlikely (shstrscnndx > SHN_HIRESERVE)
      || unlikely (shstrscnndx == SHN_XINDEX))
    {
      /* The index of the section header string sectio is too large.  */
      Elf_Scn *scn = elf_getscn (ctx->out.elf, 0);

      /* Get the header for the zeroth section.  */
      shdr = gelf_getshdr (scn, &shdr_mem);
      /* This better does not fail.  */
      assert (shdr != NULL);

      /* The sh_link field of the zeroth section header contains the value.  */
      shdr->sh_link = shstrscnndx;

      (void) gelf_update_shdr (scn, shdr);

      /* This is the sign for the overflow.  */
      ehdr->e_shstrndx = SHN_XINDEX;
    }
  else
    ehdr->e_shstrndx = elf_ndxscn (shstrscn);

  if (unlikely (gelf_update_ehdr (ctx->out.elf, ehdr) == 0))
    {
      __libasm_seterrno (ASM_E_LIBELF);
      result = -1;
    }

  /* Write out the ELF file.  */
  if (unlikely (elf_update (ctx->out.elf, ELF_C_WRITE_MMAP) < 0))
    {
      __libasm_seterrno (ASM_E_LIBELF);
      result = -1;
    }

  /* We do not need the section header and symbol string tables anymore.  */
  free (shstrtabdata->d_buf);
  if (strtabdata != NULL)
    free (strtabdata->d_buf);
  /* We might have allocated the extended symbol table index.  */
  if (xndxdata != NULL)
    free (xndxdata->d_buf);

  /* Free section groups memory.  */
  AsmScnGrp_t *scngrp = ctx->groups;
  if (scngrp != NULL)
    do
      free (elf_getdata (scngrp->scn, NULL)->d_buf);
    while ((scngrp = scngrp->next) != ctx->groups);

  /* Finalize the ELF handling.  */
  if (unlikely (elf_end (ctx->out.elf)) != 0)
    {
      __libasm_seterrno (ASM_E_LIBELF);
      result = -1;
    }

  /* Free the temporary resources.  */
  free (symtab);

  return result;
}


int
asm_end (AsmCtx_t *ctx)
{
  int result;

  if (ctx == NULL)
    /* Something went wrong earlier.  */
    return -1;

  result = unlikely (ctx->textp) ? text_end (ctx) : binary_end (ctx);
  if (result != 0)
    return result;

  /* Make the new file globally readable and user/group-writable.  */
  if (fchmod (ctx->fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) != 0)
    {
      __libasm_seterrno (ASM_E_CANNOT_CHMOD);
      return -1;
    }

  /* Rename output file.  */
  if (rename (ctx->tmp_fname, ctx->fname) != 0)
    {
      __libasm_seterrno (ASM_E_CANNOT_RENAME);
      return -1;
    }

  /* Free the resources.  */
  __libasm_finictx (ctx);

  return 0;
}


static void
free_section (AsmScn_t *scnp)
{
  void *oldp;

  if (scnp->subnext != NULL)
    free_section (scnp->subnext);

  /* This is a circular single linked list.  */
  struct AsmData *data = scnp->content;
  if (data != NULL)
    {
      while (data != scnp->content)
	{
	  oldp = data;
	  data = data->next;
	  free (oldp);
	}
      free (scnp->content);
    }
  free (scnp);
}


void
internal_function
__libasm_finictx (AsmCtx_t *ctx)
{
  /* Iterate through section table and free individual entries.  */
  AsmScn_t *scn = ctx->section_list;
  while (scn != NULL)
    {
      AsmScn_t *oldp = scn;
      scn = scn->allnext;
      free_section (oldp);
    }

  /* Free the resources of the symbol table.  */
  void *runp = NULL;
  AsmSym_t *sym;
  while ((sym = asm_symbol_tab_iterate (&ctx->symbol_tab, &runp)) != NULL)
    free (sym);
  asm_symbol_tab_free (&ctx->symbol_tab);


  /* Free section groups.  */
  AsmScnGrp_t *scngrp = ctx->groups;
  if (scngrp != NULL)
    do
      {
	AsmScnGrp_t *oldp = scngrp;

	scngrp = scngrp->next;
	free (oldp);
      }
    while (scngrp != ctx->groups);


  if (unlikely (ctx->textp))
    {
      /* Close the stream.  */
      fclose (ctx->out.file);
    }
  else
    {
      /* Close the output file.  */
      /* XXX We should test for errors here but what would we do if we'd
	 find any.  */
      (void) close (ctx->fd);

      /* And the string tables.  */
      dwelf_strtab_free (ctx->section_strtab);
      dwelf_strtab_free (ctx->symbol_strtab);
    }

  /* Initialize the lock.  */
  rwlock_fini (ctx->lock);

  /* Finally free the data structure.   */
  free (ctx);
}
