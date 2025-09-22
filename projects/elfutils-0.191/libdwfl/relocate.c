/* Relocate debug information.
   Copyright (C) 2005-2011, 2014, 2016, 2018 Red Hat, Inc.
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

#include <system.h>

#include "libelfP.h"
#include "libdwflP.h"

typedef uint8_t GElf_Byte;

/* Adjust *VALUE to add the load address of the SHNDX section.
   We update the section header in place to cache the result.  */

Dwfl_Error
internal_function
__libdwfl_relocate_value (Dwfl_Module *mod, Elf *elf, size_t *shstrndx,
			  Elf32_Word shndx, GElf_Addr *value)
{
  /* No adjustment needed for section zero, it is never loaded.
     Handle it first, just in case the ELF file has strange section
     zero flags set.  */
  if (shndx == 0)
    return DWFL_E_NOERROR;

  Elf_Scn *refscn = elf_getscn (elf, shndx);
  GElf_Shdr refshdr_mem, *refshdr = gelf_getshdr (refscn, &refshdr_mem);
  if (refshdr == NULL)
    return DWFL_E_LIBELF;

  if (refshdr->sh_addr == 0 && (refshdr->sh_flags & SHF_ALLOC))
    {
      /* This is a loaded section.  Find its actual
	 address and update the section header.  */

      if (*shstrndx == SHN_UNDEF
	  && unlikely (elf_getshdrstrndx (elf, shstrndx) < 0))
	return DWFL_E_LIBELF;

      const char *name = elf_strptr (elf, *shstrndx, refshdr->sh_name);
      if (unlikely (name == NULL))
	return DWFL_E_LIBELF;

      if ((*mod->dwfl->callbacks->section_address) (MODCB_ARGS (mod),
						    name, shndx, refshdr,
						    &refshdr->sh_addr))
	return CBFAIL;

      if (refshdr->sh_addr == (Dwarf_Addr) -1l)
	/* The callback indicated this section wasn't really loaded but we
	   don't really care.  */
	refshdr->sh_addr = 0;	/* Make no adjustment below.  */

      /* Update the in-core file's section header to show the final
	 load address (or unloadedness).  This serves as a cache,
	 so we won't get here again for the same section.  */
      if (likely (refshdr->sh_addr != 0)
	  && unlikely (! gelf_update_shdr (refscn, refshdr)))
	return DWFL_E_LIBELF;
    }

  if (refshdr->sh_flags & SHF_ALLOC)
    /* Apply the adjustment.  */
    *value += dwfl_adjusted_address (mod, refshdr->sh_addr);

  return DWFL_E_NOERROR;
}


/* Cache used by relocate_getsym.  */
struct reloc_symtab_cache
{
  Elf *symelf;
  Elf_Data *symdata;
  Elf_Data *symxndxdata;
  Elf_Data *symstrdata;
  size_t symshstrndx;
  size_t strtabndx;
};
#define RELOC_SYMTAB_CACHE(cache)	\
  struct reloc_symtab_cache cache =	\
    { NULL, NULL, NULL, NULL, SHN_UNDEF, SHN_UNDEF }

/* This is just doing dwfl_module_getsym, except that we must always use
   the symbol table in RELOCATED itself when it has one, not MOD->symfile.  */
static Dwfl_Error
relocate_getsym (Dwfl_Module *mod,
		 Elf *relocated, struct reloc_symtab_cache *cache,
		 int symndx, GElf_Sym *sym, GElf_Word *shndx)
{
  if (cache->symdata == NULL)
    {
      if (mod->symfile == NULL || mod->symfile->elf != relocated)
	{
	  /* We have to look up the symbol table in the file we are
	     relocating, if it has its own.  These reloc sections refer to
	     the symbol table in this file, and a symbol table in the main
	     file might not match.  However, some tools did produce ET_REL
	     .debug files with relocs but no symtab of their own.  */
	  Elf_Scn *scn = NULL;
	  while ((scn = elf_nextscn (relocated, scn)) != NULL)
	    {
	      GElf_Shdr shdr_mem, *shdr = gelf_getshdr (scn, &shdr_mem);
	      if (shdr != NULL)
		{
		  /* We need uncompressed data.  */
		  if ((shdr->sh_type == SHT_SYMTAB
		       || shdr->sh_type == SHT_SYMTAB_SHNDX)
		      && (shdr->sh_flags & SHF_COMPRESSED) != 0)
		    if (elf_compress (scn, 0, 0) < 0)
		      return DWFL_E_LIBELF;

		  switch (shdr->sh_type)
		    {
		    default:
		      continue;
		    case SHT_SYMTAB:
		      cache->symelf = relocated;
		      cache->symdata = elf_getdata (scn, NULL);
		      cache->strtabndx = shdr->sh_link;
		      if (unlikely (cache->symdata == NULL))
			return DWFL_E_LIBELF;
		      break;
		    case SHT_SYMTAB_SHNDX:
		      cache->symxndxdata = elf_getdata (scn, NULL);
		      if (unlikely (cache->symxndxdata == NULL))
			return DWFL_E_LIBELF;
		      break;
		    }
		}
	      if (cache->symdata != NULL && cache->symxndxdata != NULL)
		break;
	    }
	}
      if (cache->symdata == NULL)
	{
	  /* We might not have looked for a symbol table file yet,
	     when coming from __libdwfl_relocate_section.  */
	  if (unlikely (mod->symfile == NULL)
	      && unlikely (INTUSE(dwfl_module_getsymtab) (mod) < 0))
	    return dwfl_errno ();

	  /* The symbol table we have already cached is the one from
	     the file being relocated, so it's what we need.  Or else
	     this is an ET_REL .debug file with no .symtab of its own;
	     the symbols refer to the section indices in the main file.  */
	  cache->symelf = mod->symfile->elf;
	  cache->symdata = mod->symdata;
	  cache->symxndxdata = mod->symxndxdata;
	  cache->symstrdata = mod->symstrdata;
	}
    }

  if (unlikely (gelf_getsymshndx (cache->symdata, cache->symxndxdata,
				  symndx, sym, shndx) == NULL))
    return DWFL_E_LIBELF;

  if (sym->st_shndx != SHN_XINDEX)
    *shndx = sym->st_shndx;

  switch (sym->st_shndx)
    {
    case SHN_ABS:
    case SHN_UNDEF:
      return DWFL_E_NOERROR;

    case SHN_COMMON:
      sym->st_value = 0;	/* Value is size, not helpful. */
      return DWFL_E_NOERROR;
    }

  return __libdwfl_relocate_value (mod, cache->symelf, &cache->symshstrndx,
				   *shndx, &sym->st_value);
}

/* Handle an undefined symbol.  We really only support ET_REL for Linux
   kernel modules, and offline archives.  The behavior of the Linux module
   loader is very simple and easy to mimic.  It only matches magically
   exported symbols, and we match any defined symbols.  But we get the same
   answer except when the module's symbols are undefined and would prevent
   it from being loaded.  */
static Dwfl_Error
resolve_symbol (Dwfl_Module *referer, struct reloc_symtab_cache *symtab,
		GElf_Sym *sym, GElf_Word shndx)
{
  /* First we need its name.  */
  if (sym->st_name != 0)
    {
      if (symtab->symstrdata == NULL)
	{
	  /* Cache the strtab for this symtab.  */
	  assert (referer->symfile == NULL
		  || referer->symfile->elf != symtab->symelf);

	  Elf_Scn *scn = elf_getscn (symtab->symelf, symtab->strtabndx);
	  if (scn == NULL)
	    return DWFL_E_LIBELF;

	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    return DWFL_E_LIBELF;

	  if (symtab->symshstrndx == SHN_UNDEF
	      && elf_getshdrstrndx (symtab->symelf, &symtab->symshstrndx) < 0)
	    return DWFL_E_LIBELF;

	  const char *sname = elf_strptr (symtab->symelf, symtab->symshstrndx,
					  shdr->sh_name);
	  if (sname == NULL)
	    return DWFL_E_LIBELF;

	  /* If the section is already decompressed, that isn't an error.  */
	  if (startswith (sname, ".zdebug"))
	    elf_compress_gnu (scn, 0, 0);

	  if ((shdr->sh_flags & SHF_COMPRESSED) != 0)
	    if (elf_compress (scn, 0, 0) < 0)
	      return DWFL_E_LIBELF;

	  symtab->symstrdata = elf_getdata (scn, NULL);
	  if (unlikely (symtab->symstrdata == NULL
			|| symtab->symstrdata->d_buf == NULL))
	    return DWFL_E_LIBELF;
	}
      if (unlikely (sym->st_name >= symtab->symstrdata->d_size))
	return DWFL_E_BADSTROFF;

      const char *name = symtab->symstrdata->d_buf;
      name += sym->st_name;

      for (Dwfl_Module *m = referer->dwfl->modulelist; m != NULL; m = m->next)
	if (m != referer)
	  {
	    /* Get this module's symtab.
	       If we got a fresh error reading the table, report it.
	       If we just have no symbols in this module, no harm done.  */
	    if (m->symdata == NULL
		&& m->symerr == DWFL_E_NOERROR
		&& INTUSE(dwfl_module_getsymtab) (m) < 0
		&& m->symerr != DWFL_E_NO_SYMTAB)
	      return m->symerr;

	    for (size_t ndx = 1; ndx < m->syments; ++ndx)
	      {
		sym = gelf_getsymshndx (m->symdata, m->symxndxdata,
					ndx, sym, &shndx);
		if (unlikely (sym == NULL))
		  return DWFL_E_LIBELF;
		if (sym->st_shndx != SHN_XINDEX)
		  shndx = sym->st_shndx;

		/* We are looking for a defined global symbol with a name.  */
		if (shndx == SHN_UNDEF || shndx == SHN_COMMON
		    || GELF_ST_BIND (sym->st_info) == STB_LOCAL
		    || sym->st_name == 0)
		  continue;

		/* Get this candidate symbol's name.  */
		if (unlikely (sym->st_name >= m->symstrdata->d_size))
		  return DWFL_E_BADSTROFF;
		const char *n = m->symstrdata->d_buf;
		n += sym->st_name;

		/* Does the name match?  */
		if (strcmp (name, n))
		  continue;

		/* We found it!  */
		if (shndx == SHN_ABS) /* XXX maybe should apply bias? */
		  return DWFL_E_NOERROR;

		if (m->e_type != ET_REL)
		  {
		    sym->st_value = dwfl_adjusted_st_value (m, m->symfile->elf,
							    sym->st_value);
		    return DWFL_E_NOERROR;
		  }

		/* In an ET_REL file, the symbol table values are relative
		   to the section, not to the module's load base.  */
		size_t symshstrndx = SHN_UNDEF;
		return __libdwfl_relocate_value (m, m->symfile->elf,
						 &symshstrndx,
						 shndx, &sym->st_value);
	      }
	  }
    }

  return DWFL_E_RELUNDEF;
}

/* Apply one relocation.  Returns true for any invalid data.  */
static Dwfl_Error
relocate (Dwfl_Module * const mod,
          Elf * const relocated,
          struct reloc_symtab_cache * const reloc_symtab,
          Elf_Data * const tdata,
          const GElf_Ehdr * const ehdr,
          GElf_Addr offset,
          const GElf_Sxword *addend,
          int rtype,
          int symndx)
{
    /* First see if this is a reloc we can handle.
       If we are skipping it, don't bother resolving the symbol.  */

    if (unlikely (rtype == 0))
      /* In some odd situations, the linker can leave R_*_NONE relocs
	 behind.  This is probably bogus ld -r behavior, but the only
	 cases it's known to appear in are harmless: DWARF data
	 referring to addresses in a section that has been discarded.
	 So we just pretend it's OK without further relocation.  */
      return DWFL_E_NOERROR;

    int addsub = 0;
    Elf_Type type = ebl_reloc_simple_type (mod->ebl, rtype, &addsub);
    if (unlikely (type == ELF_T_NUM))
      return DWFL_E_BADRELTYPE;

    /* First, resolve the symbol to an absolute value.  */
    GElf_Addr value;

    if (symndx == STN_UNDEF)
      /* When strip removes a section symbol referring to a
	 section moved into the debuginfo file, it replaces
	 that symbol index in relocs with STN_UNDEF.  We
	 don't actually need the symbol, because those relocs
	 are always references relative to the nonallocated
	 debugging sections, which start at zero.  */
      value = 0;
    else
      {
	GElf_Sym sym;
	GElf_Word shndx;
	Dwfl_Error error = relocate_getsym (mod, relocated, reloc_symtab,
					    symndx, &sym, &shndx);
	if (unlikely (error != DWFL_E_NOERROR))
	  return error;

	if (shndx == SHN_UNDEF || shndx == SHN_COMMON)
	  {
	    /* Maybe we can figure it out anyway.  */
	    error = resolve_symbol (mod, reloc_symtab, &sym, shndx);
	    if (error != DWFL_E_NOERROR
		&& !(error == DWFL_E_RELUNDEF && shndx == SHN_COMMON))
	      return error;
	  }

	value = sym.st_value;
      }

    /* These are the types we can relocate.  */
#define TYPES		DO_TYPE (BYTE, Byte); DO_TYPE (HALF, Half);	\
    DO_TYPE (WORD, Word); DO_TYPE (SWORD, Sword);			\
    DO_TYPE (XWORD, Xword); DO_TYPE (SXWORD, Sxword)
    size_t size;
    switch (type)
      {
#define DO_TYPE(NAME, Name)			\
	case ELF_T_##NAME:			\
	  if (addsub != 0 && addend == NULL)	\
	    /* These do not make sense with SHT_REL.  */ \
	    return DWFL_E_BADRELTYPE;		\
	  size = sizeof (GElf_##Name);		\
	break
	TYPES;
#undef DO_TYPE
      default:
	return DWFL_E_BADRELTYPE;
      }

    if (offset > tdata->d_size || tdata->d_size - offset < size)
      return DWFL_E_BADRELOFF;

#define DO_TYPE(NAME, Name) GElf_##Name Name;
    union { TYPES; } tmpbuf;
#undef DO_TYPE
    Elf_Data tmpdata =
      {
	.d_type = type,
	.d_buf = &tmpbuf,
	.d_size = size,
	.d_version = EV_CURRENT,
      };
    Elf_Data rdata =
      {
	.d_type = type,
	.d_buf = tdata->d_buf + offset,
	.d_size = size,
	.d_version = EV_CURRENT,
      };

    /* XXX check for overflow? */
    if (addend)
      {
	/* For the addend form, we have the value already.  */
	value += *addend;
	/* For ADD/SUB relocations we need to fetch the section
	   contents.  */
	if (addsub != 0)
	  {
	    Elf_Data *d = gelf_xlatetom (relocated, &tmpdata, &rdata,
					 ehdr->e_ident[EI_DATA]);
	    if (d == NULL)
	      return DWFL_E_LIBELF;
	    assert (d == &tmpdata);
	  }
	switch (type)
	  {
#define DO_TYPE(NAME, Name)			\
	    case ELF_T_##NAME:			\
	      if (addsub != 0)			\
		tmpbuf.Name += value * addsub;	\
	      else				\
		tmpbuf.Name = value;		\
	    break
	    TYPES;
#undef DO_TYPE
	  default:
	    abort ();
	  }
      }
    else
      {
	/* Extract the original value and apply the reloc.  */
	Elf_Data *d = gelf_xlatetom (relocated, &tmpdata, &rdata,
				     ehdr->e_ident[EI_DATA]);
	if (d == NULL)
	  return DWFL_E_LIBELF;
	assert (d == &tmpdata);
	switch (type)
	  {
#define DO_TYPE(NAME, Name)				\
	    case ELF_T_##NAME:				\
	      tmpbuf.Name += (GElf_##Name) value;	\
	    break
	    TYPES;
#undef DO_TYPE
	  default:
	    abort ();
	  }
      }

    /* Now convert the relocated datum back to the target
       format.  This will write into rdata.d_buf, which
       points into the raw section data being relocated.  */
    Elf_Data *s = gelf_xlatetof (relocated, &rdata, &tmpdata,
				 ehdr->e_ident[EI_DATA]);
    if (s == NULL)
      return DWFL_E_LIBELF;
    assert (s == &rdata);

    /* We have applied this relocation!  */
    return DWFL_E_NOERROR;
}

static inline void
check_badreltype (bool *first_badreltype,
                  Dwfl_Module *mod,
                  Dwfl_Error *result)
{
  if (*first_badreltype)
    {
       *first_badreltype = false;
       if (ebl_get_elfmachine (mod->ebl) == EM_NONE)
          /* This might be because ebl_openbackend failed to find
             any libebl_CPU.so library.  Diagnose that clearly.  */
          *result = DWFL_E_UNKNOWN_MACHINE;
     }
}

static Dwfl_Error
relocate_section (Dwfl_Module *mod, Elf *relocated, const GElf_Ehdr *ehdr,
		  size_t shstrndx, struct reloc_symtab_cache *reloc_symtab,
		  Elf_Scn *scn, GElf_Shdr *shdr,
		  Elf_Scn *tscn, bool debugscn, bool partial)
{
  /* First, fetch the name of the section these relocations apply to.
     Then try to decompress both relocation and target section.  */
  GElf_Shdr tshdr_mem;
  GElf_Shdr *tshdr = gelf_getshdr (tscn, &tshdr_mem);
  if (tshdr == NULL)
    return DWFL_E_LIBELF;

  const char *tname = elf_strptr (relocated, shstrndx, tshdr->sh_name);
  if (tname == NULL)
    return DWFL_E_LIBELF;

  if (debugscn && ! ebl_debugscn_p (mod->ebl, tname))
    /* This relocation section is not for a debugging section.
       Nothing to do here.  */
    return DWFL_E_NOERROR;

  if (startswith (tname, ".zdebug"))
    elf_compress_gnu (tscn, 0, 0);

  if ((tshdr->sh_flags & SHF_COMPRESSED) != 0)
    if (elf_compress (tscn, 0, 0) < 0)
      return DWFL_E_LIBELF;

  /* Reload Shdr in case section was just decompressed.  */
  tshdr = gelf_getshdr (tscn, &tshdr_mem);
  if (tshdr == NULL)
    return DWFL_E_LIBELF;

  if (unlikely (tshdr->sh_type == SHT_NOBITS)
      || unlikely (tshdr->sh_size == 0))
    /* No contents to relocate.  */
    return DWFL_E_NOERROR;

  const char *sname = elf_strptr (relocated, shstrndx, shdr->sh_name);
  if (sname == NULL)
    return DWFL_E_LIBELF;

  if (startswith (sname, ".zdebug"))
    elf_compress_gnu (scn, 0, 0);

  if ((shdr->sh_flags & SHF_COMPRESSED) != 0)
    if (elf_compress (scn, 0, 0) < 0)
      return DWFL_E_LIBELF;

  /* Reload Shdr in case section was just decompressed.  */
  GElf_Shdr shdr_mem;
  shdr = gelf_getshdr (scn, &shdr_mem);
  if (shdr == NULL)
    return DWFL_E_LIBELF;

  /* Fetch the section data that needs the relocations applied.  */
  Elf_Data *tdata = elf_rawdata (tscn, NULL);
  if (tdata == NULL)
    return DWFL_E_LIBELF;

  /* If either the section that needs the relocation applied, or the
     section that the relocations come from overlap one of the ehdrs,
     shdrs or phdrs data then we refuse to do the relocations.  It
     isn't illegal for ELF section data to overlap the header data,
     but updating the (relocation) data might corrupt the in-memory
     libelf headers causing strange corruptions or errors.

     This is only an issue if the ELF is mmapped and the section data
     comes from the mmapped region (is not malloced or decompressed).
  */
  if (relocated->map_address != NULL)
    {
      size_t ehsize = gelf_fsize (relocated, ELF_T_EHDR, 1, EV_CURRENT);
      if (unlikely (shdr->sh_offset < ehsize
		    || tshdr->sh_offset < ehsize))
	return DWFL_E_BADELF;

      GElf_Off shdrs_start = ehdr->e_shoff;
      size_t shnums;
      if (elf_getshdrnum (relocated, &shnums) < 0)
	return DWFL_E_LIBELF;
      /* Overflows will have been checked by elf_getshdrnum/get|rawdata.  */
      size_t shentsize = gelf_fsize (relocated, ELF_T_SHDR, 1, EV_CURRENT);
      GElf_Off shdrs_end = shdrs_start + shnums * shentsize;
      if (unlikely (shdrs_start < shdr->sh_offset + shdr->sh_size
		    && shdr->sh_offset < shdrs_end))
	if ((scn->flags & ELF_F_MALLOCED) == 0)
	  return DWFL_E_BADELF;

      if (unlikely (shdrs_start < tshdr->sh_offset + tshdr->sh_size
		    && tshdr->sh_offset < shdrs_end))
	if ((tscn->flags & ELF_F_MALLOCED) == 0)
	  return DWFL_E_BADELF;

      GElf_Off phdrs_start = ehdr->e_phoff;
      size_t phnums;
      if (elf_getphdrnum (relocated, &phnums) < 0)
	return DWFL_E_LIBELF;
      if (phdrs_start != 0 && phnums != 0)
	{
	  /* Overflows will have been checked by elf_getphdrnum/get|rawdata.  */
	  size_t phentsize = gelf_fsize (relocated, ELF_T_PHDR, 1, EV_CURRENT);
	  GElf_Off phdrs_end = phdrs_start + phnums * phentsize;
	  if (unlikely (phdrs_start < shdr->sh_offset + shdr->sh_size
			&& shdr->sh_offset < phdrs_end))
	    if ((scn->flags & ELF_F_MALLOCED) == 0)
	      return DWFL_E_BADELF;

	  if (unlikely (phdrs_start < tshdr->sh_offset + tshdr->sh_size
			&& tshdr->sh_offset < phdrs_end))
	    if ((tscn->flags & ELF_F_MALLOCED) == 0)
	      return DWFL_E_BADELF;
	}
    }

  /* Fetch the relocation section and apply each reloc in it.  */
  Elf_Data *reldata = elf_getdata (scn, NULL);
  if (reldata == NULL)
    return DWFL_E_LIBELF;

  Dwfl_Error result = DWFL_E_NOERROR;
  bool first_badreltype = true;

  size_t sh_entsize
    = gelf_fsize (relocated, shdr->sh_type == SHT_REL ? ELF_T_REL : ELF_T_RELA,
		  1, EV_CURRENT);
  size_t nrels = shdr->sh_size / sh_entsize;
  size_t complete = 0;
  if (shdr->sh_type == SHT_REL)
    for (size_t relidx = 0; !result && relidx < nrels; ++relidx)
      {
	GElf_Rel rel_mem, *r = gelf_getrel (reldata, relidx, &rel_mem);
	if (r == NULL)
	  return DWFL_E_LIBELF;
	result = relocate (mod, relocated, reloc_symtab, tdata, ehdr,
			   r->r_offset, NULL,
			   GELF_R_TYPE (r->r_info),
			   GELF_R_SYM (r->r_info));
	check_badreltype (&first_badreltype, mod, &result);
	if (partial)
	  switch (result)
	    {
	    case DWFL_E_NOERROR:
	      /* We applied the relocation.  Elide it.  */
	      memset (&rel_mem, 0, sizeof rel_mem);
	      if (unlikely (gelf_update_rel (reldata, relidx, &rel_mem) == 0))
		return DWFL_E_LIBELF;
	      ++complete;
	      break;
	    case DWFL_E_BADRELTYPE:
	    case DWFL_E_RELUNDEF:
	      /* We couldn't handle this relocation.  Skip it.  */
	      result = DWFL_E_NOERROR;
	      break;
	    default:
	      break;
	    }
      }
  else
    for (size_t relidx = 0; !result && relidx < nrels; ++relidx)
      {
	GElf_Rela rela_mem, *r = gelf_getrela (reldata, relidx,
					       &rela_mem);
	if (r == NULL)
	  return DWFL_E_LIBELF;
	result = relocate (mod, relocated, reloc_symtab, tdata, ehdr,
			   r->r_offset, &r->r_addend,
			   GELF_R_TYPE (r->r_info),
			   GELF_R_SYM (r->r_info));
	check_badreltype (&first_badreltype, mod, &result);
	if (partial)
	  switch (result)
	    {
	    case DWFL_E_NOERROR:
	      /* We applied the relocation.  Elide it.  */
	      memset (&rela_mem, 0, sizeof rela_mem);
	      if (unlikely (gelf_update_rela (reldata, relidx,
					      &rela_mem) == 0))
		return DWFL_E_LIBELF;
	      ++complete;
	      break;
	    case DWFL_E_BADRELTYPE:
	    case DWFL_E_RELUNDEF:
	      /* We couldn't handle this relocation.  Skip it.  */
	      result = DWFL_E_NOERROR;
	      break;
	    default:
	      break;
	    }
      }

  if (likely (result == DWFL_E_NOERROR))
    {
      if (!partial || complete == nrels)
	/* Mark this relocation section as being empty now that we have
	   done its work.  This affects unstrip -R, so e.g. it emits an
	   empty .rela.debug_info along with a .debug_info that has
	   already been fully relocated.  */
	nrels = 0;
      else if (complete != 0)
	{
	  /* We handled some of the relocations but not all.
	     We've zeroed out the ones we processed.
	     Now remove them from the section.  */

	  size_t next = 0;
	  if (shdr->sh_type == SHT_REL)
	    for (size_t relidx = 0; relidx < nrels; ++relidx)
	      {
		GElf_Rel rel_mem;
		GElf_Rel *r = gelf_getrel (reldata, relidx, &rel_mem);
		if (unlikely (r == NULL))
		  return DWFL_E_LIBELF;
		if (r->r_info != 0 || r->r_offset != 0)
		  {
		    if (next != relidx)
		      if (unlikely (gelf_update_rel (reldata, next, r) == 0))
			return DWFL_E_LIBELF;
		    ++next;
		  }
	      }
	  else
	    for (size_t relidx = 0; relidx < nrels; ++relidx)
	      {
		GElf_Rela rela_mem;
		GElf_Rela *r = gelf_getrela (reldata, relidx, &rela_mem);
		if (unlikely (r == NULL))
		  return DWFL_E_LIBELF;
		if (r->r_info != 0 || r->r_offset != 0 || r->r_addend != 0)
		  {
		    if (next != relidx)
		      if (unlikely (gelf_update_rela (reldata, next, r) == 0))
			return DWFL_E_LIBELF;
		    ++next;
		  }
	      }
	  nrels = next;
	}

      shdr->sh_size = reldata->d_size = nrels * sh_entsize;
      if (unlikely (gelf_update_shdr (scn, shdr) == 0))
	return DWFL_E_LIBELF;
    }

  return result;
}

Dwfl_Error
internal_function
__libdwfl_relocate (Dwfl_Module *mod, Elf *debugfile, bool debug)
{
  assert (mod->e_type == ET_REL);

  GElf_Ehdr ehdr_mem;
  const GElf_Ehdr *ehdr = gelf_getehdr (debugfile, &ehdr_mem);
  if (ehdr == NULL)
    return DWFL_E_LIBELF;

  size_t d_shstrndx;
  if (elf_getshdrstrndx (debugfile, &d_shstrndx) < 0)
    return DWFL_E_LIBELF;

  RELOC_SYMTAB_CACHE (reloc_symtab);

  /* Look at each section in the debuginfo file, and process the
     relocation sections for debugging sections.  */
  Dwfl_Error result = DWFL_E_NOERROR;
  Elf_Scn *scn = NULL;
  while (result == DWFL_E_NOERROR
	 && (scn = elf_nextscn (debugfile, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      if (unlikely (shdr == NULL))
	return DWFL_E_LIBELF;

      if ((shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA)
	  && shdr->sh_size != 0)
	{
	  /* It's a relocation section.  */

	  Elf_Scn *tscn = elf_getscn (debugfile, shdr->sh_info);
	  if (unlikely (tscn == NULL))
	    result = DWFL_E_LIBELF;
	  else
	    result = relocate_section (mod, debugfile, ehdr, d_shstrndx,
				       &reloc_symtab, scn, shdr, tscn,
				       debug, true /* partial always OK. */);
	}
    }

  return result;
}

Dwfl_Error
internal_function
__libdwfl_relocate_section (Dwfl_Module *mod, Elf *relocated,
			    Elf_Scn *relocscn, Elf_Scn *tscn, bool partial)
{
  GElf_Ehdr ehdr_mem;
  GElf_Shdr shdr_mem;

  RELOC_SYMTAB_CACHE (reloc_symtab);

  size_t shstrndx;
  if (elf_getshdrstrndx (relocated, &shstrndx) < 0)
    return DWFL_E_LIBELF;

  Dwfl_Error result = __libdwfl_module_getebl (mod);
  if (unlikely (result != DWFL_E_NOERROR))
    return result;

  GElf_Ehdr *ehdr = gelf_getehdr (relocated, &ehdr_mem);
  if (unlikely (ehdr == NULL))
    return DWFL_E_LIBELF;

  GElf_Shdr *shdr = gelf_getshdr (relocscn, &shdr_mem);
  if (unlikely (shdr == NULL))
    return DWFL_E_LIBELF;

  return relocate_section (mod, relocated, ehdr, shstrndx, &reloc_symtab,
			   relocscn, shdr, tscn, false, partial);
}
