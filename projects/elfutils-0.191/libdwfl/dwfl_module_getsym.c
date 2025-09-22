/* Find debugging and symbol information for a module in libdwfl.
   Copyright (C) 2006-2014 Red Hat, Inc.
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

const char *
internal_function
__libdwfl_getsym (Dwfl_Module *mod, int ndx, GElf_Sym *sym, GElf_Addr *addr,
		  GElf_Word *shndxp, Elf **elfp, Dwarf_Addr *biasp,
		  bool *resolved, bool adjust_st_value)
{
  if (unlikely (mod == NULL))
    return NULL;

  if (unlikely (mod->symdata == NULL))
    {
      int result = INTUSE(dwfl_module_getsymtab) (mod);
      if (result < 0)
	return NULL;
    }

  /* All local symbols should come before all global symbols.  If we
     have an auxiliary table make sure all the main locals come first,
     then all aux locals, then all main globals and finally all aux globals.
     And skip the auxiliary table zero undefined entry.  */
  GElf_Word shndx;
  int tndx = ndx;
  int skip_aux_zero = (mod->syments > 0 && mod->aux_syments > 0) ? 1 : 0;
  Elf *elf;
  Elf_Data *symdata;
  Elf_Data *symxndxdata;
  Elf_Data *symstrdata;
  if (mod->aux_symdata == NULL
      || ndx < mod->first_global)
    {
      /* main symbol table (locals).  */
      tndx = ndx;
      elf = mod->symfile->elf;
      symdata = mod->symdata;
      symxndxdata = mod->symxndxdata;
      symstrdata = mod->symstrdata;
    }
  else if (ndx < mod->first_global + mod->aux_first_global - skip_aux_zero)
    {
      /* aux symbol table (locals).  */
      tndx = ndx - mod->first_global + skip_aux_zero;
      elf = mod->aux_sym.elf;
      symdata = mod->aux_symdata;
      symxndxdata = mod->aux_symxndxdata;
      symstrdata = mod->aux_symstrdata;
    }
  else if ((size_t) ndx < mod->syments + mod->aux_first_global - skip_aux_zero)
    {
      /* main symbol table (globals).  */
      tndx = ndx - mod->aux_first_global + skip_aux_zero;
      elf = mod->symfile->elf;
      symdata = mod->symdata;
      symxndxdata = mod->symxndxdata;
      symstrdata = mod->symstrdata;
    }
  else
    {
      /* aux symbol table (globals).  */
      tndx = ndx - mod->syments + skip_aux_zero;
      elf = mod->aux_sym.elf;
      symdata = mod->aux_symdata;
      symxndxdata = mod->aux_symxndxdata;
      symstrdata = mod->aux_symstrdata;
    }
  sym = gelf_getsymshndx (symdata, symxndxdata, tndx, sym, &shndx);

  if (unlikely (sym == NULL))
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return NULL;
    }

  if (sym->st_shndx != SHN_XINDEX)
    shndx = sym->st_shndx;

  /* Figure out whether this symbol points into an SHF_ALLOC section.  */
  bool alloc = true;
  if ((shndxp != NULL || mod->e_type != ET_REL)
      && (sym->st_shndx == SHN_XINDEX
	  || (sym->st_shndx < SHN_LORESERVE && sym->st_shndx != SHN_UNDEF)))
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (elf_getscn (elf, shndx), &shdr_mem);
      alloc = unlikely (shdr == NULL) || (shdr->sh_flags & SHF_ALLOC);
    }

  /* In case of an value in an allocated section the main Elf Ebl
     might know where the real value is (e.g. for function
     descriptors).  */

  char *ident;
  GElf_Addr st_value = sym->st_value & ebl_func_addr_mask (mod->ebl);
  *resolved = false;
  if (! adjust_st_value && mod->e_type != ET_REL && alloc
      && (GELF_ST_TYPE (sym->st_info) == STT_FUNC
	  || (GELF_ST_TYPE (sym->st_info) == STT_GNU_IFUNC
	      && (ident = elf_getident (elf, NULL)) != NULL
	      && ident[EI_OSABI] == ELFOSABI_LINUX)))
    {
      if (likely (__libdwfl_module_getebl (mod) == DWFL_E_NOERROR))
	{
	  if (elf != mod->main.elf)
	    {
	      st_value = dwfl_adjusted_st_value (mod, elf, st_value);
	      st_value = dwfl_deadjust_st_value (mod, mod->main.elf, st_value);
	    }

	  *resolved = ebl_resolve_sym_value (mod->ebl, &st_value);
	  if (! *resolved)
	    st_value = sym->st_value;
	}
    }

  if (shndxp != NULL)
    /* Yield -1 in case of a non-SHF_ALLOC section.  */
    *shndxp = alloc ? shndx : (GElf_Word) -1;

  switch (sym->st_shndx)
    {
    case SHN_ABS:		/* XXX sometimes should use bias?? */
    case SHN_UNDEF:
    case SHN_COMMON:
      break;

    default:
      if (mod->e_type == ET_REL)
	{
	  /* In an ET_REL file, the symbol table values are relative
	     to the section, not to the module's load base.  */
	  size_t symshstrndx = SHN_UNDEF;
	  Dwfl_Error result = __libdwfl_relocate_value (mod, elf,
							&symshstrndx,
							shndx, &st_value);
	  if (unlikely (result != DWFL_E_NOERROR))
	    {
	      __libdwfl_seterrno (result);
	      return NULL;
	    }
	}
      else if (alloc)
	/* Apply the bias to the symbol value.  */
	st_value = dwfl_adjusted_st_value (mod,
					   *resolved ? mod->main.elf : elf,
					   st_value);
      break;
    }

  if (adjust_st_value)
    sym->st_value = st_value;

  if (addr != NULL)
    *addr = st_value;

  if (unlikely (sym->st_name >= symstrdata->d_size))
    {
      __libdwfl_seterrno (DWFL_E_BADSTROFF);
      return NULL;
    }
  if (elfp)
    *elfp = elf;
  if (biasp)
    *biasp = dwfl_adjusted_st_value (mod, elf, 0);
  return (const char *) symstrdata->d_buf + sym->st_name;
}

const char *
dwfl_module_getsym_info (Dwfl_Module *mod, int ndx,
			 GElf_Sym *sym, GElf_Addr *addr,
			 GElf_Word *shndxp,
			 Elf **elfp, Dwarf_Addr *bias)
{
  bool resolved;
  return __libdwfl_getsym (mod, ndx, sym, addr, shndxp, elfp, bias,
			   &resolved, false);
}
INTDEF (dwfl_module_getsym_info)

const char *
dwfl_module_getsym (Dwfl_Module *mod, int ndx,
		    GElf_Sym *sym, GElf_Word *shndxp)
{
  bool resolved;
  return __libdwfl_getsym (mod, ndx, sym, NULL, shndxp, NULL, NULL,
			   &resolved, true);
}
INTDEF (dwfl_module_getsym)
