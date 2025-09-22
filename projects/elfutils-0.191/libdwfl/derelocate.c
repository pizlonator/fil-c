/* Recover relocatibility for addresses computed from debug information.
   Copyright (C) 2005-2010, 2013, 2015 Red Hat, Inc.
   This file is part of elfutils.

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

#include "libdwflP.h"

struct dwfl_relocation
{
  size_t count;
  struct
  {
    Elf_Scn *scn;
    Elf_Scn *relocs;
    const char *name;
    GElf_Addr start, end;
  } refs[0];
};


struct secref
{
  struct secref *next;
  Elf_Scn *scn;
  Elf_Scn *relocs;
  const char *name;
  GElf_Addr start, end;
};

static int
compare_secrefs (const void *a, const void *b)
{
  struct secref *const *p1 = a;
  struct secref *const *p2 = b;

  /* No signed difference calculation is correct here, since the
     terms are unsigned and could be more than INT64_MAX apart.  */
  if ((*p1)->start < (*p2)->start)
    return -1;
  if ((*p1)->start > (*p2)->start)
    return 1;

  if ((*p1)->end < (*p2)->end)
    return -1;
  if ((*p1)->end > (*p2)->end)
    return 1;

  /* Same start/end, then just compare which section came first.  */
  return elf_ndxscn ((*p1)->scn) - elf_ndxscn ((*p2)->scn);
}

static int
cache_sections (Dwfl_Module *mod)
{
  if (likely (mod->reloc_info != NULL))
    return mod->reloc_info->count;

  struct secref *refs = NULL;
  size_t nrefs = 0;

  size_t shstrndx;
  if (unlikely (elf_getshdrstrndx (mod->main.elf, &shstrndx) < 0))
    {
    elf_error:
      __libdwfl_seterrno (DWFL_E_LIBELF);
      nrefs = -1;
      goto free_refs;
    }

  bool check_reloc_sections = false;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (mod->main.elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (shdr == NULL)
	goto elf_error;

      if ((shdr->sh_flags & SHF_ALLOC) && shdr->sh_addr == 0
	  && mod->e_type == ET_REL)
	{
	  /* This section might not yet have been looked at.  */
	  if (__libdwfl_relocate_value (mod, mod->main.elf, &shstrndx,
					elf_ndxscn (scn),
					&shdr->sh_addr) != DWFL_E_NOERROR)
	    continue;
	  shdr = gelf_getshdr (scn, &shdr_mem);
	  if (unlikely (shdr == NULL))
	    goto elf_error;
	}

      if (shdr->sh_flags & SHF_ALLOC)
	{
	  const char *name = elf_strptr (mod->main.elf, shstrndx,
					 shdr->sh_name);
	  if (unlikely (name == NULL))
	    goto elf_error;

	  struct secref *newref = malloc (sizeof *newref);
	  if (unlikely (newref == NULL))
	    {
	    nomem:
	      __libdwfl_seterrno (DWFL_E_NOMEM);
	      nrefs = -1;
	      goto free_refs;
	    }

	  newref->scn = scn;
	  newref->relocs = NULL;
	  newref->name = name;
	  newref->start = dwfl_adjusted_address (mod, shdr->sh_addr);
	  newref->end = newref->start + shdr->sh_size;
	  newref->next = refs;
	  refs = newref;
	  ++nrefs;
	}

      if (mod->e_type == ET_REL
	  && shdr->sh_size != 0
	  && (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA)
	  && mod->dwfl->callbacks->section_address != NULL)
	{
	  if (shdr->sh_info < elf_ndxscn (scn))
	    {
	      /* We've already looked at the section these relocs apply to.  */
	      Elf_Scn *tscn = elf_getscn (mod->main.elf, shdr->sh_info);
	      if (likely (tscn != NULL))
		for (struct secref *sec = refs; sec != NULL; sec = sec->next)
		  if (sec->scn == tscn)
		    {
		      sec->relocs = scn;
		      break;
		    }
	    }
	  else
	    /* We'll have to do a second pass.  */
	    check_reloc_sections = true;
	}
    }

  mod->reloc_info = malloc (offsetof (struct dwfl_relocation, refs[nrefs]));
  if (unlikely (mod->reloc_info == NULL))
    goto nomem;

  struct secref **sortrefs = malloc (nrefs * sizeof sortrefs[0]);
  if (unlikely (sortrefs == NULL))
    goto nomem;

  for (size_t i = nrefs; i-- > 0; refs = refs->next)
    sortrefs[i] = refs;
  assert (refs == NULL);

  qsort (sortrefs, nrefs, sizeof sortrefs[0], &compare_secrefs);

  mod->reloc_info->count = nrefs;
  for (size_t i = 0; i < nrefs; ++i)
    {
      mod->reloc_info->refs[i].name = sortrefs[i]->name;
      mod->reloc_info->refs[i].scn = sortrefs[i]->scn;
      mod->reloc_info->refs[i].relocs = sortrefs[i]->relocs;
      mod->reloc_info->refs[i].start = sortrefs[i]->start;
      mod->reloc_info->refs[i].end = sortrefs[i]->end;
      free (sortrefs[i]);
    }

  free (sortrefs);

  if (unlikely (check_reloc_sections))
    {
      /* There was a reloc section that preceded its target section.
	 So we have to scan again now that we have cached all the
	 possible target sections we care about.  */

      scn = NULL;
      while ((scn = elf_nextscn (mod->main.elf, scn)) != NULL)
	{
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    goto elf_error;

      	  if (shdr->sh_size != 0
	      && (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA))
	    {
	      Elf_Scn *tscn = elf_getscn (mod->main.elf, shdr->sh_info);
	      if (likely (tscn != NULL))
		for (size_t i = 0; i < nrefs; ++i)
		  if (mod->reloc_info->refs[i].scn == tscn)
		    {
		      mod->reloc_info->refs[i].relocs = scn;
		      break;
		    }
	    }
	}
    }

free_refs:
  while (refs != NULL)
    {
      struct secref *ref = refs;
      refs = ref->next;
      free (ref);
    }

  return nrefs;
}


int
dwfl_module_relocations (Dwfl_Module *mod)
{
  if (mod == NULL)
    return -1;

  switch (mod->e_type)
    {
    case ET_REL:
      return cache_sections (mod);

    case ET_DYN:
      return 1;

    case ET_EXEC:
      assert (mod->main.vaddr == mod->low_addr);
      break;
    }

  return 0;
}

const char *
dwfl_module_relocation_info (Dwfl_Module *mod, unsigned int idx,
			     Elf32_Word *shndxp)
{
  if (mod == NULL)
    return NULL;

  switch (mod->e_type)
    {
    case ET_REL:
      break;

    case ET_DYN:
      if (idx != 0)
	return NULL;
      if (shndxp)
	*shndxp = SHN_ABS;
      return "";

    default:
      return NULL;
    }

  if (cache_sections (mod) < 0)
    return NULL;

  struct dwfl_relocation *sections = mod->reloc_info;

  if (idx >= sections->count)
    return NULL;

  if (shndxp)
    *shndxp = elf_ndxscn (sections->refs[idx].scn);

  return sections->refs[idx].name;
}

/* Check that MOD is valid and make sure its relocation has been done.  */
static bool
check_module (Dwfl_Module *mod)
{
  if (mod == NULL)
    return true;

  if (INTUSE(dwfl_module_getsymtab) (mod) < 0)
    {
      Dwfl_Error error = dwfl_errno ();
      if (error != DWFL_E_NO_SYMTAB)
	{
	  __libdwfl_seterrno (error);
	  return true;
	}
    }

  if (mod->dw == NULL)
    {
      Dwarf_Addr bias;
      if (INTUSE(dwfl_module_getdwarf) (mod, &bias) == NULL)
	{
	  Dwfl_Error error = dwfl_errno ();
	  if (error != DWFL_E_NO_DWARF)
	    {
	      __libdwfl_seterrno (error);
	      return true;
	    }
	}
    }

  return false;
}

/* Find the index in MOD->reloc_info.refs containing *ADDR.  */
static int
find_section (Dwfl_Module *mod, Dwarf_Addr *addr)
{
  if (cache_sections (mod) < 0)
    return -1;

  struct dwfl_relocation *sections = mod->reloc_info;

  /* The sections are sorted by address, so we can use binary search.  */
  size_t l = 0, u = sections->count;
  while (l < u)
    {
      size_t idx = (l + u) / 2;
      if (*addr < sections->refs[idx].start)
	u = idx;
      else if (*addr > sections->refs[idx].end)
	l = idx + 1;
      else
	{
	  /* Consider the limit of a section to be inside it, unless it's
	     inside the next one.  A section limit address can appear in
	     line records.  */
	  if (*addr == sections->refs[idx].end
	      && idx + 1 < sections->count
	      && *addr == sections->refs[idx + 1].start)
	    ++idx;

	  *addr -= sections->refs[idx].start;
	  return idx;
	}
    }

  __libdwfl_seterrno (DWFL_E (LIBDW, DWARF_E_NO_MATCH));
  return -1;
}

size_t
internal_function
__libdwfl_find_section_ndx (Dwfl_Module *mod, Dwarf_Addr *addr)
{
  int idx = find_section (mod, addr);
  if (unlikely (idx == -1))
    return SHN_UNDEF;

  return elf_ndxscn (mod->reloc_info->refs[idx].scn);
}

int
dwfl_module_relocate_address (Dwfl_Module *mod, Dwarf_Addr *addr)
{
  if (unlikely (check_module (mod)))
    return -1;

  switch (mod->e_type)
    {
    case ET_REL:
      return find_section (mod, addr);

    case ET_DYN:
      /* All relative to first and only relocation base: module start.  */
      *addr -= mod->low_addr;
      break;

    default:
      /* Already absolute, dwfl_module_relocations returned zero.  We
	 shouldn't really have been called, but it's a harmless no-op.  */
      break;
    }

  return 0;
}
INTDEF (dwfl_module_relocate_address)

Elf_Scn *
dwfl_module_address_section (Dwfl_Module *mod, Dwarf_Addr *address,
			     Dwarf_Addr *bias)
{
  if (check_module (mod))
    return NULL;

  int idx = find_section (mod, address);
  if (idx < 0)
    return NULL;

  if (mod->reloc_info->refs[idx].relocs != NULL)
    {
      assert (mod->e_type == ET_REL);

      Elf_Scn *tscn = mod->reloc_info->refs[idx].scn;
      Elf_Scn *relocscn = mod->reloc_info->refs[idx].relocs;
      Dwfl_Error result = __libdwfl_relocate_section (mod, mod->main.elf,
						      relocscn, tscn, true);
      if (likely (result == DWFL_E_NOERROR))
	mod->reloc_info->refs[idx].relocs = NULL;
      else
	{
	  __libdwfl_seterrno (result);
	  return NULL;
	}
    }

  *bias = dwfl_adjusted_address (mod, 0);
  return mod->reloc_info->refs[idx].scn;
}
INTDEF (dwfl_module_address_section)
