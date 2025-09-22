/* Find debugging and symbol information for a module in libdwfl.
   Copyright (C) 2005-2013 Red Hat, Inc.
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

struct search_state
{
  Dwfl_Module *mod;
  GElf_Addr addr;

  GElf_Sym *closest_sym;
  bool adjust_st_value;
  GElf_Word addr_shndx;
  Elf *addr_symelf;

  /* Keep track of the closest symbol we have seen so far.
     Here we store only symbols with nonzero st_size.  */
  const char *closest_name;
  GElf_Addr closest_value;
  GElf_Word closest_shndx;
  Elf *closest_elf;

  /* Keep track of an eligible symbol with st_size == 0 as a fallback.  */
  const char *sizeless_name;
  GElf_Sym sizeless_sym;
  GElf_Addr sizeless_value;
  GElf_Word sizeless_shndx;
  Elf *sizeless_elf;

  /* Keep track of the lowest address a relevant sizeless symbol could have.  */
  GElf_Addr min_label;
};

/* Return true iff we consider ADDR to lie in the same section as SYM.  */
static inline bool
same_section (struct search_state *state,
	      GElf_Addr value, Elf *symelf, GElf_Word shndx)
{
  /* For absolute symbols and the like, only match exactly.  */
  if (shndx >= SHN_LORESERVE)
    return value == state->addr;

  /* If value might not be st_value, the shndx of the symbol might
      not match the section of the value. Explicitly look both up.  */
  if (! state->adjust_st_value)
    {
      Dwarf_Addr v;
      if (state->addr_shndx == SHN_UNDEF)
        {
          v = state->addr;
          state->addr_shndx = __libdwfl_find_section_ndx (state->mod, &v);
        }

      v = value;
      return state->addr_shndx == __libdwfl_find_section_ndx (state->mod, &v);
    }

  /* Figure out what section ADDR lies in.  */
  if (state->addr_shndx == SHN_UNDEF || state->addr_symelf != symelf)
    {
      GElf_Addr mod_addr = dwfl_deadjust_st_value (state->mod, symelf,
						   state->addr);
      Elf_Scn *scn = NULL;
      state->addr_shndx = SHN_ABS;
      state->addr_symelf = symelf;
      while ((scn = elf_nextscn (symelf, scn)) != NULL)
        {
          GElf_Shdr shdr_mem;
          GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
          if (likely (shdr != NULL)
              && mod_addr >= shdr->sh_addr
              && mod_addr < shdr->sh_addr + shdr->sh_size)
            {
              state->addr_shndx = elf_ndxscn (scn);
              break;
            }
        }
    }

  return shndx == state->addr_shndx && state->addr_symelf == symelf;
}

/* Return GELF_ST_BIND as higher-is-better integer.  */
static inline int
binding_value (const GElf_Sym *symp)
{
  switch (GELF_ST_BIND (symp->st_info))
    {
    case STB_GLOBAL:
      return 3;
    case STB_WEAK:
      return 2;
    case STB_LOCAL:
      return 1;
    default:
      return 0;
    }
}

/* Try one symbol and associated value from the search table.  */
static inline void
try_sym_value (struct search_state *state,
               GElf_Addr value, GElf_Sym *sym,
               const char *name, GElf_Word shndx,
               Elf *elf, bool resolved)
{
    /* Even if we don't choose this symbol, its existence excludes
       any sizeless symbol (assembly label) that is below its upper
       bound.  */
    if (value + sym->st_size > state->min_label)
      state->min_label = value + sym->st_size;

    if (sym->st_size == 0 || state->addr - value < sym->st_size)
      {
	/* This symbol is a better candidate than the current one
	   if it's closer to ADDR or is global when it was local.  */
	if (state->closest_name == NULL
	    || state->closest_value < value
	    || binding_value (state->closest_sym) < binding_value (sym))
	  {
	    if (sym->st_size != 0)
	      {
		*state->closest_sym = *sym;
		state->closest_value = value;
		state->closest_shndx = shndx;
		state->closest_elf = elf;
		state->closest_name = name;
	      }
	    else if (state->closest_name == NULL
		     && value >= state->min_label
		     && same_section (state, value,
				      resolved ? state->mod->main.elf : elf,
				      shndx))
	      {
		/* Handwritten assembly symbols sometimes have no
		   st_size.  If no symbol with proper size includes
		   the address, we'll use the closest one that is in
		   the same section as ADDR.  */
		state->sizeless_sym = *sym;
		state->sizeless_value = value;
		state->sizeless_shndx = shndx;
		state->sizeless_elf = elf;
		state->sizeless_name = name;
	      }
	  }
	/* When the beginning of its range is no closer,
	   the end of its range might be.  Otherwise follow
	   GELF_ST_BIND preference.  If all are equal prefer
	   the first symbol found.  */
	else if (sym->st_size != 0
		 && state->closest_value == value
		 && ((state->closest_sym->st_size > sym->st_size
		      && (binding_value (state->closest_sym)
			  <= binding_value (sym)))
		     || (state->closest_sym->st_size >= sym->st_size
			 && (binding_value (state->closest_sym)
			     < binding_value (sym)))))
	  {
	    *state->closest_sym = *sym;
	    state->closest_value = value;
	    state->closest_shndx = shndx;
	    state->closest_elf = elf;
	    state->closest_name = name;
	  }
      }
}

/* Look through the symbol table for a matching symbol.  */
static inline void
search_table (struct search_state *state, int start, int end)
{
      for (int i = start; i < end; ++i)
	{
	  GElf_Sym sym;
	  GElf_Addr value;
	  GElf_Word shndx;
	  Elf *elf;
	  bool resolved;
	  const char *name = __libdwfl_getsym (state->mod, i, &sym, &value,
					       &shndx, &elf, NULL,
					       &resolved,
					       state->adjust_st_value);
	  if (name != NULL && name[0] != '\0'
	      && sym.st_shndx != SHN_UNDEF
	      && value <= state->addr
	      && GELF_ST_TYPE (sym.st_info) != STT_SECTION
	      && GELF_ST_TYPE (sym.st_info) != STT_FILE
	      && GELF_ST_TYPE (sym.st_info) != STT_TLS)
	    {
	      try_sym_value (state, value, &sym, name, shndx, elf, resolved);

	      /* If this is an addrinfo variant and the value could be
		 resolved then also try matching the (adjusted) st_value.  */
	      if (resolved && state->mod->e_type != ET_REL)
		{
		  GElf_Addr adjusted_st_value;
		  adjusted_st_value = dwfl_adjusted_st_value (state->mod, elf,
							      sym.st_value);
		  if (value != adjusted_st_value
		      && adjusted_st_value <= state->addr)
		    try_sym_value (state, adjusted_st_value, &sym, name, shndx,
				   elf, false);
		}
	    }
	}
}

/* Returns the name of the symbol "closest" to ADDR.
   Never returns symbols at addresses above ADDR.

   Wrapper for old dwfl_module_addrsym and new dwfl_module_addrinfo.
   adjust_st_value set to true returns adjusted SYM st_value, set to false
   it will not adjust SYM at all, but does match against resolved values.   */
static const char *
__libdwfl_addrsym (Dwfl_Module *_mod, GElf_Addr _addr, GElf_Off *off,
		   GElf_Sym *_closest_sym, GElf_Word *shndxp,
		   Elf **elfp, Dwarf_Addr *biasp, bool _adjust_st_value)
{
  int syments = INTUSE(dwfl_module_getsymtab) (_mod);
  if (syments < 0)
    return NULL;

  struct search_state state =
    {
      .addr = _addr,
      .mod = _mod,
      .closest_sym = _closest_sym,
      .adjust_st_value = _adjust_st_value,
      .addr_shndx = SHN_UNDEF,
      .addr_symelf = NULL,
      .closest_name = NULL,
      .closest_value = 0,
      .closest_shndx = SHN_UNDEF,
      .closest_elf = NULL,
      .sizeless_name = NULL,
      .sizeless_sym = { 0, 0, 0, 0, 0, SHN_UNDEF },
      .sizeless_value = 0,
      .sizeless_shndx = SHN_UNDEF,
      .sizeless_elf = NULL,
      .min_label = 0
    };

  /* First go through global symbols.  mod->first_global and
     mod->aux_first_global are setup by dwfl_module_getsymtab to the
     index of the first global symbol in those symbol tables.  Both
     are non-zero when the table exist, except when there is only a
     dynsym table loaded through phdrs, then first_global is zero and
     there will be no auxiliary table.  All symbols with local binding
     come first in the symbol table, then all globals.  The zeroth,
     null entry, in the auxiliary table is skipped if there is a main
     table.  */
  int first_global = INTUSE (dwfl_module_getsymtab_first_global) (state.mod);
  if (first_global < 0)
    return NULL;
  search_table (&state, first_global == 0 ? 1 : first_global, syments);

  /* If we found nothing searching the global symbols, then try the locals.
     Unless we have a global sizeless symbol that matches exactly.  */
  if (state.closest_name == NULL && first_global > 1
      && (state.sizeless_name == NULL || state.sizeless_value != state.addr))
    search_table (&state, 1, first_global);

  /* If we found no proper sized symbol to use, fall back to the best
     candidate sizeless symbol we found, if any.  */
  if (state.closest_name == NULL
      && state.sizeless_name != NULL
      && state.sizeless_value >= state.min_label)
    {
      *state.closest_sym = state.sizeless_sym;
      state.closest_value = state.sizeless_value;
      state.closest_shndx = state.sizeless_shndx;
      state.closest_elf = state.sizeless_elf;
      state.closest_name = state.sizeless_name;
    }

  *off = state.addr - state.closest_value;

  if (shndxp != NULL)
    *shndxp = state.closest_shndx;
  if (elfp != NULL)
    *elfp = state.closest_elf;
  if (biasp != NULL)
    *biasp = dwfl_adjusted_st_value (state.mod, state.closest_elf, 0);
  return state.closest_name;
}


const char *
dwfl_module_addrsym (Dwfl_Module *mod, GElf_Addr addr,
		     GElf_Sym *closest_sym, GElf_Word *shndxp)
{
  GElf_Off off;
  return __libdwfl_addrsym (mod, addr, &off, closest_sym, shndxp,
			    NULL, NULL, true);
}
INTDEF (dwfl_module_addrsym)

const char
*dwfl_module_addrinfo (Dwfl_Module *mod, GElf_Addr address,
		       GElf_Off *offset, GElf_Sym *sym,
		       GElf_Word *shndxp, Elf **elfp, Dwarf_Addr *bias)
{
  return __libdwfl_addrsym (mod, address, offset, sym, shndxp, elfp, bias,
			    false);
}
INTDEF (dwfl_module_addrinfo)
