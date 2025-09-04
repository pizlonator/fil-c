/* Inline functions for dynamic linking.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <dl-machine.h>
#include <elf.h>

#ifdef RESOLVE_MAP
/* We pass reloc_addr as a pointer to void, as opposed to a pointer to
   ElfW(Addr), because not all architectures can assume that the
   relocated address is properly aligned, whereas the compiler is
   entitled to assume that a pointer to a type is properly aligned for
   the type.  Even if we cast the pointer back to some other type with
   less strict alignment requirements, the compiler might still
   remember that the pointer was originally more aligned, thereby
   optimizing away alignment tests or using word instructions for
   copying memory, breaking the very code written to handle the
   unaligned cases.  */
# if ! ELF_MACHINE_NO_REL
static inline void __attribute__((always_inline))
elf_machine_rel (struct link_map *map, struct r_scope_elem *scope[],
		 const ElfW(Rel) *reloc, const ElfW(Sym) *sym,
		 const struct r_found_version *version,
		 void *const reloc_addr, int skip_ifunc);
static inline void __attribute__((always_inline))
elf_machine_rel_relative (ElfW(Addr) l_addr, const ElfW(Rel) *reloc,
			  void *const reloc_addr);
# endif
# if ! ELF_MACHINE_NO_RELA
static inline void __attribute__((always_inline))
elf_machine_rela (struct link_map *map, struct r_scope_elem *scope[],
		  const ElfW(Rela) *reloc, const ElfW(Sym) *sym,
		  const struct r_found_version *version, void *const reloc_addr,
		  int skip_ifunc);
static inline void __attribute__((always_inline))
elf_machine_rela_relative (ElfW(Addr) l_addr, const ElfW(Rela) *reloc,
			   void *const reloc_addr);
# endif
# if ELF_MACHINE_NO_RELA || defined ELF_MACHINE_PLT_REL
static inline void __attribute__((always_inline))
elf_machine_lazy_rel (struct link_map *map, struct r_scope_elem *scope[],
		      ElfW(Addr) l_addr, const ElfW(Rel) *reloc,
		      int skip_ifunc);
# else
static inline void __attribute__((always_inline))
elf_machine_lazy_rel (struct link_map *map, struct r_scope_elem *scope[],
		      ElfW(Addr) l_addr, const ElfW(Rela) *reloc,
		      int skip_ifunc);
# endif
#endif

#ifdef RESOLVE_MAP

/* Get the definitions of `elf_dynamic_do_rel' and `elf_dynamic_do_rela'.
   These functions are almost identical, so we use cpp magic to avoid
   duplicating their code.  It cannot be done in a more general function
   because we must be able to completely inline.  */

/* On some machines, notably SPARC, DT_REL* includes DT_JMPREL in its
   range.  Note that according to the ELF spec, this is completely legal!

   We are guaranteed that we have one of three situations.  Either DT_JMPREL
   comes immediately after DT_REL*, or there is overlap and DT_JMPREL
   consumes precisely the very end of the DT_REL*, or DT_JMPREL and DT_REL*
   are completely separate and there is a gap between them.  */

# define _ELF_DYNAMIC_DO_RELOC(RELOC, reloc, map, scope, do_lazy, skip_ifunc, test_rel) \
  do {									      \
    struct { ElfW(Addr) start, size;					      \
	     __typeof (((ElfW(Dyn) *) 0)->d_un.d_val) nrelative; int lazy; }  \
      ranges[2] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };			      \
									      \
    /* With DT_RELR, DT_RELA/DT_REL can have zero value.  */		      \
    if ((map)->l_info[DT_##RELOC] != NULL				      \
	&& (map)->l_info[DT_##RELOC]->d_un.d_ptr != 0)			      \
      {									      \
	ranges[0].start = D_PTR ((map), l_info[DT_##RELOC]);		      \
	ranges[0].size = (map)->l_info[DT_##RELOC##SZ]->d_un.d_val;	      \
	if (map->l_info[VERSYMIDX (DT_##RELOC##COUNT)] != NULL)		      \
	  ranges[0].nrelative						      \
	    = map->l_info[VERSYMIDX (DT_##RELOC##COUNT)]->d_un.d_val;	      \
      }									      \
    if ((map)->l_info[DT_PLTREL]					      \
	&& (!test_rel || (map)->l_info[DT_PLTREL]->d_un.d_val == DT_##RELOC)) \
      {									      \
	ElfW(Addr) start = D_PTR ((map), l_info[DT_JMPREL]);		      \
	ElfW(Addr) size = (map)->l_info[DT_PLTRELSZ]->d_un.d_val;	      \
									      \
	if (ranges[0].start == 0)					      \
	  ranges[0].start = start;					      \
	if (ranges[0].start + ranges[0].size == (start + size))		      \
	  ranges[0].size -= size;					      \
	if (!(do_lazy)							      \
	    && (ranges[0].start + ranges[0].size) == start)		      \
	  {								      \
	    /* Combine processing the sections.  */			      \
	    ranges[0].size += size;					      \
	  }								      \
	else								      \
	  {								      \
	    ranges[1].start = start;					      \
	    ranges[1].size = size;					      \
	    ranges[1].lazy = (do_lazy);					      \
	  }								      \
      }									      \
									      \
      for (int ranges_index = 0; ranges_index < 2; ++ranges_index)	      \
        elf_dynamic_do_##reloc ((map), scope,				      \
				ranges[ranges_index].start,		      \
				ranges[ranges_index].size,		      \
				ranges[ranges_index].nrelative,		      \
				ranges[ranges_index].lazy,		      \
				skip_ifunc);				      \
  } while (0)

# if ELF_MACHINE_NO_REL || ELF_MACHINE_NO_RELA
#  define _ELF_CHECK_REL 0
# else
#  define _ELF_CHECK_REL 1
# endif

# if ! ELF_MACHINE_NO_REL
#  include "do-rel.h"
#  define ELF_DYNAMIC_DO_REL(map, scope, lazy, skip_ifunc)	      \
  _ELF_DYNAMIC_DO_RELOC (REL, Rel, map, scope, lazy, skip_ifunc, _ELF_CHECK_REL)
# else
#  define ELF_DYNAMIC_DO_REL(map, scope, lazy, skip_ifunc) /* Nothing to do.  */
# endif

# if ! ELF_MACHINE_NO_RELA
#  define DO_RELA
#  include "do-rel.h"
#  define ELF_DYNAMIC_DO_RELA(map, scope, lazy, skip_ifunc)	      \
  _ELF_DYNAMIC_DO_RELOC (RELA, Rela, map, scope, lazy, skip_ifunc, _ELF_CHECK_REL)
# else
#  define ELF_DYNAMIC_DO_RELA(map, scope, lazy, skip_ifunc) /* Nothing to do.  */
# endif

# define ELF_DYNAMIC_DO_RELR(map)					      \
  do {									      \
    ElfW(Addr) l_addr = (map)->l_addr, *where = 0;			      \
    const ElfW(Relr) *r, *end;						      \
    if ((map)->l_info[DT_RELR] == NULL)					      \
      break;								      \
    r = (const ElfW(Relr) *)D_PTR((map), l_info[DT_RELR]);		      \
    end = (const ElfW(Relr) *)((const char *)r +			      \
                               (map)->l_info[DT_RELRSZ]->d_un.d_val);	      \
    for (; r < end; r++)						      \
      {									      \
	ElfW(Relr) entry = *r;						      \
	if ((entry & 1) == 0)						      \
	  {								      \
	    where = (ElfW(Addr) *)(l_addr + entry);			      \
	    *where++ += l_addr;						      \
	  }								      \
	else 								      \
	  {								      \
	    for (long int i = 0; (entry >>= 1) != 0; i++)		      \
	      if ((entry & 1) != 0)					      \
		where[i] += l_addr;					      \
	    where += CHAR_BIT * sizeof(ElfW(Relr)) - 1;			      \
	  }								      \
      }									      \
  } while (0);

# ifndef ELF_DYNAMIC_AFTER_RELOC
#  define ELF_DYNAMIC_AFTER_RELOC(map, lazy)
# endif

/* This can't just be an inline function because GCC is too dumb
   to inline functions containing inlines themselves.  */
# ifdef RTLD_BOOTSTRAP
#  define DO_RTLD_BOOTSTRAP 1
# else
#  define DO_RTLD_BOOTSTRAP 0
# endif
# define ELF_DYNAMIC_RELOCATE(map, scope, lazy, consider_profile, skip_ifunc) \
  do {									      \
    int edr_lazy = elf_machine_runtime_setup ((map), (scope), (lazy),	      \
					      (consider_profile));	      \
    if (((map) != &GL(dl_rtld_map) || DO_RTLD_BOOTSTRAP))		      \
      ELF_DYNAMIC_DO_RELR (map);					      \
    ELF_DYNAMIC_DO_REL ((map), (scope), edr_lazy, skip_ifunc);		      \
    ELF_DYNAMIC_DO_RELA ((map), (scope), edr_lazy, skip_ifunc);		      \
    ELF_DYNAMIC_AFTER_RELOC ((map), (edr_lazy));			      \
  } while (0)

#endif
