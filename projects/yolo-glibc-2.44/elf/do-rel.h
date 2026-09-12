/* Do relocations for ELF dynamic linking.
   Copyright (C) 1995-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <ldsodefs.h>

/* This file may be included twice, to define both
   `elf_dynamic_do_rel' and `elf_dynamic_do_rela'.  */

#ifdef DO_RELA
# define elf_dynamic_do_Rel		elf_dynamic_do_Rela
# define elf_dynamic_do_Rel_irelative	elf_dynamic_do_Rela_irelative
# define elf_dynamic_is_Rel_irelative	elf_dynamic_is_Rela_irelative
# define elf_dynamic_Rel_audit_symbind	elf_dynamic_Rela_audit_symbind
# define Rel				Rela
# define elf_machine_rel		elf_machine_rela
# define elf_machine_rel_relative	elf_machine_rela_relative
#endif

#ifndef DO_ELF_MACHINE_REL_RELATIVE
# define DO_ELF_MACHINE_REL_RELATIVE(map, l_addr, relative) \
  elf_machine_rel_relative (l_addr, relative,				      \
			    (void *) (l_addr + relative->r_offset))
#endif

static __always_inline bool
elf_dynamic_is_Rel_irelative (const ElfW(Rel) *reloc, const ElfW(Sym) *sym)
{
#ifdef ELF_MACHINE_IRELATIVE
  const unsigned int r_type = ELFW (R_TYPE) (reloc->r_info);
  return ((sym != NULL
	   && ELFW(ST_TYPE) (sym->st_info) == STT_GNU_IFUNC
	   && sym->st_shndx != SHN_UNDEF)
	  || r_type == ELF_MACHINE_IRELATIVE
# ifdef ELF_MACHINE_IRELATIVE_PLT
	  /* Some ports resolve an IFUNC PLT slot for a local resolver with a
	     dedicated reloc that carries no symboli.  */
	  || r_type == ELF_MACHINE_IRELATIVE_PLT
# endif
	  );
#else
  return false;
#endif
}

static __always_inline void
elf_dynamic_Rel_audit_symbind (struct link_map *map,
			       struct r_scope_elem *scope[],
			       const ElfW(Rel) *reloc, const ElfW(Sym) *sym,
			       const struct r_found_version *rversion,
			       void *r_addr_arg)
{
#if defined SHARED
  if (ELFW(R_TYPE) (reloc->r_info) == ELF_MACHINE_JMP_SLOT
      && GLRO(dl_naudit) > 0)
    {
      struct link_map *sym_map
	= RESOLVE_MAP (map, scope, &sym, rversion, ELF_MACHINE_JMP_SLOT);
      if (sym != NULL)
	_dl_audit_symbind (map, NULL, reloc, sym, r_addr_arg, sym_map, false);
    }
#endif
}

/* Perform the relocations in MAP on the running program image as specified
   by RELTAG, SZTAG.  If LAZY is nonzero, this is the first pass on PLT
   relocations; they should be set up to call _dl_runtime_resolve, rather
   than fully resolved now.

   IRELATIVE entries are always skipped (non-bootstrap); they are handled
   separately by elf_dynamic_do_Rel_irelative after all other relocations
   for both .rel.dyn and .rel.plt have been processed.  */

static inline void __attribute__ ((always_inline))
elf_dynamic_do_Rel (struct link_map *map, struct r_scope_elem *scope[],
		    ElfW(Addr) reladdr, ElfW(Addr) relsize,
		    __typeof (((ElfW(Dyn) *) 0)->d_un.d_val) nrelative,
		    int lazy)
{
  const ElfW(Rel) *relative = (const void *) reladdr;
  const ElfW(Rel) *r = relative + nrelative;
  const ElfW(Rel) *end = (const void *) (reladdr + relsize);
  ElfW(Addr) l_addr = map->l_addr;
  const ElfW(Sym) *const symtab
      = (const void *) D_PTR (map, l_info[DT_SYMTAB]);

#ifdef RTLD_BOOTSTRAP
  for (; relative < r; ++relative)
    DO_ELF_MACHINE_REL_RELATIVE (map, l_addr, relative);

  const ElfW (Half) *const version
      = (const void *) D_PTR (map, l_info[VERSYMIDX (DT_VERSYM)]);
  for (; r < end; ++r)
    {
      ElfW (Half) ndx = version[ELFW (R_SYM) (r->r_info)] & 0x7fff;
      const ElfW (Sym) *sym = &symtab[ELFW (R_SYM) (r->r_info)];
      void *const r_addr_arg = (void *) (l_addr + r->r_offset);
      const struct r_found_version *rversion = &map->l_versions[ndx];

      elf_machine_rel (map, scope, r, sym, rversion, r_addr_arg, 0);
    }
#else /* !RTLD_BOOTSTRAP */
#if !defined DO_RELA || !defined ELF_MACHINE_PLT_REL
  /* We never bind lazily during ld.so bootstrap.  Unfortunately gcc is
     not clever enough to see through all the function calls to realize
     that.  */
  if (lazy)
    {
      /* Doing lazy PLT relocations; they need very little info.  */
      for (; r < end; ++r)
	{
	  const ElfW (Sym) *sym = &symtab[ELFW (R_SYM) (r->r_info)];
	  if (elf_dynamic_is_Rel_irelative (r, sym))
	    continue;
	  elf_machine_lazy_rel (map, scope, l_addr, r, 0);
	}
    }
  else
#endif
    {
      if (!is_rtld_link_map (map)) /* Already done in rtld itself.  */
# if !defined DO_RELA || defined ELF_MACHINE_REL_RELATIVE
	/* Rela platforms get the offset from r_addend and this must
	   be copied in the relocation address.  Therefore we can skip
	   the relative relocations only if this is for rel
	   relocations or rela relocations if they are computed as
	   memory_loc += l_addr...  */
	if (l_addr != 0)
# endif
	  for (; relative < r; ++relative)
	    DO_ELF_MACHINE_REL_RELATIVE (map, l_addr, relative);

      if (map->l_info[VERSYMIDX (DT_VERSYM)])
	{
	  const ElfW(Half) *const version =
	    (const void *) D_PTR (map, l_info[VERSYMIDX (DT_VERSYM)]);

	  for (; r < end; ++r)
	    {
	      ElfW(Half) ndx = version[ELFW(R_SYM) (r->r_info)] & 0x7fff;
	      const ElfW(Sym) *sym = &symtab[ELFW(R_SYM) (r->r_info)];
	      void *const r_addr_arg = (void *) (l_addr + r->r_offset);
	      const struct r_found_version *rversion = &map->l_versions[ndx];

	      if (elf_dynamic_is_Rel_irelative (r, sym))
		continue;
	      elf_machine_rel (map, scope, r, sym, rversion, r_addr_arg, 0);
	      elf_dynamic_Rel_audit_symbind (map, scope, r, sym, rversion,
					     r_addr_arg);
	    }
	}
      else
	{
	  for (; r < end; ++r)
	    {
	      const ElfW(Sym) *sym = &symtab[ELFW(R_SYM) (r->r_info)];
	      void *const r_addr_arg = (void *) (l_addr + r->r_offset);

	      if (elf_dynamic_is_Rel_irelative (r, sym))
		continue;
	      elf_machine_rel (map, scope, r, sym, NULL, r_addr_arg, 0);
	      elf_dynamic_Rel_audit_symbind (map, scope, r, sym, NULL,
					     r_addr_arg);
	    }
	}
    }
#endif /* !RTLD_BOOTSTRAP */
}

/* Process only IRELATIVE entries (and other relocations targeting a defined
   STT_GNU_IFUNC symbol) in the relocation range [reladdr, reladdr+relsize).
   The first NRELATIVE entries are R_*_RELATIVE and are skipped without
   inspection.  When lazy is non-zero the PLT lazy-binding path
   (elf_machine_lazy_rel) is used, otherwise the full non-lazy path
   (elf_machine_rel) is used.

   Called by _ELF_DYNAMIC_DO_RELOC after all non-IRELATIVE relocations have
   been processed for both .rela.dyn and .rela.plt, so that IRELATIVE
   resolvers may call PLT stubs safely regardless of which section the linker
   placed R_*_IRELATIVE in.  */
static __always_inline void
elf_dynamic_do_Rel_irelative (struct link_map *map,
			      struct r_scope_elem *scope[],
			      ElfW(Addr) reladdr, ElfW(Addr) relsize,
			      __typeof (((ElfW(Dyn) *) 0)->d_un.d_val) nrelative,
			      int lazy, int skip_ifunc)
{
# ifdef ELF_MACHINE_IRELATIVE
  const ElfW(Rel) *r = (const ElfW(Rel) *) reladdr + nrelative;
  const ElfW(Rel) *end = (const void *) (reladdr + relsize);
  ElfW(Addr) l_addr = map->l_addr;
  const ElfW(Sym) *const symtab = (const void *) D_PTR (map, l_info[DT_SYMTAB]);

  if (lazy)
    {
      for (; r < end; ++r)
	{
	  const ElfW (Sym) *sym = &symtab[ELFW (R_SYM) (r->r_info)];
	  if (!elf_dynamic_is_Rel_irelative (r, sym))
	    continue;
	  elf_machine_lazy_rel (map, scope, l_addr, r, skip_ifunc);
	}
    }
  else
    {
      if (map->l_info[VERSYMIDX (DT_VERSYM)])
	{
	  const ElfW(Half) *const version =
	    (const void *) D_PTR (map, l_info[VERSYMIDX (DT_VERSYM)]);

	  for (; r < end; ++r)
	    {
	      const ElfW(Sym) *sym = &symtab[ELFW(R_SYM) (r->r_info)];
	      void *const r_addr_arg = (void *) (l_addr + r->r_offset);
	      if (!elf_dynamic_is_Rel_irelative (r, sym))
		continue;

	      ElfW(Half) ndx = version[ELFW(R_SYM) (r->r_info)] & 0x7fff;
	      const struct r_found_version *rversion = &map->l_versions[ndx];
	      elf_machine_rel (map, scope, r, sym, rversion, r_addr_arg,
			       skip_ifunc);
	      elf_dynamic_Rel_audit_symbind (map, scope, r, sym, rversion,
					     r_addr_arg);
	    }
	}
      else
	{
	  for (; r < end; ++r)
	    {
	      const ElfW(Sym) *sym = &symtab[ELFW(R_SYM) (r->r_info)];
	      void *const r_addr_arg = (void *) (l_addr + r->r_offset);
	      if (!elf_dynamic_is_Rel_irelative (r, sym))
		continue;

	      elf_machine_rel (map, scope, r, sym, NULL, r_addr_arg,
			       skip_ifunc);
	      elf_dynamic_Rel_audit_symbind (map, scope, r, sym, NULL,
					     r_addr_arg);
	    }
	}
    }
# endif
}

#undef elf_dynamic_do_Rel
#undef elf_dynamic_do_Rel_irelative
#undef elf_dynamic_is_Rel_irelative
#undef elf_dynamic_Rel_audit_symbind
#undef Rel
#undef elf_machine_rel
#undef elf_machine_rel_relative
#undef DO_ELF_MACHINE_REL_RELATIVE
#undef DO_RELA
