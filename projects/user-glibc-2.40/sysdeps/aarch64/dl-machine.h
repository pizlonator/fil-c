/* Copyright (C) 1995-2024 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef dl_machine_h
#define dl_machine_h

#define ELF_MACHINE_NAME "aarch64"

#include <sysdep.h>
#include <tls.h>
#include <dl-tlsdesc.h>
#include <dl-static-tls.h>
#include <dl-irel.h>
#include <dl-machine-rel.h>
#include <cpu-features.c>

/* Translate a processor specific dynamic tag to the index in l_info array.  */
#define DT_AARCH64(x) (DT_AARCH64_##x - DT_LOPROC + DT_NUM)

/* Return nonzero iff ELF header is compatible with the running host.  */
static inline int __attribute__ ((unused))
elf_machine_matches_host (const ElfW(Ehdr) *ehdr)
{
  return ehdr->e_machine == EM_AARCH64;
}

/* Return the run-time load address of the shared object.  */

static inline ElfW(Addr) __attribute__ ((unused))
elf_machine_load_address (void)
{
  extern const ElfW(Ehdr) __ehdr_start attribute_hidden;
  return (ElfW(Addr)) &__ehdr_start;
}

/* Return the link-time address of _DYNAMIC.  */

static inline ElfW(Addr) __attribute__ ((unused))
elf_machine_dynamic (void)
{
  extern ElfW(Dyn) _DYNAMIC[] attribute_hidden;
  return (ElfW(Addr)) _DYNAMIC - elf_machine_load_address ();
}

/* Set up the loaded object described by L so its unrelocated PLT
   entries will jump to the on-demand fixup code in dl-runtime.c.  */

static inline int __attribute__ ((unused))
elf_machine_runtime_setup (struct link_map *l, struct r_scope_elem *scope[],
			   int lazy, int profile)
{
  if (l->l_info[DT_JMPREL] && lazy)
    {
      ElfW(Addr) *got;
      extern void _dl_runtime_resolve (ElfW(Word));

      got = (ElfW(Addr) *) D_PTR (l, l_info[DT_PLTGOT]);
      if (got[1])
	{
	  l->l_mach.plt = got[1] + l->l_addr;
	}
      got[1] = (ElfW(Addr)) l;

      /* The got[2] entry contains the address of a function which gets
	 called to get the address of a so far unresolved function and
	 jump to it.  The profiling extension of the dynamic linker allows
	 to intercept the calls to collect information.  In this case we
	 don't store the address in the GOT so that all future calls also
	 end in this function.  */
#ifdef SHARED
      extern void _dl_runtime_profile (ElfW(Word));
      if ( profile)
	{
	   got[2] = (ElfW(Addr)) &_dl_runtime_profile;

	  if (GLRO(dl_profile) != NULL
	      && _dl_name_match_p (GLRO(dl_profile), l))
	    /* Say that we really want profiling and the timers are
	       started.  */
	    GL(dl_profile_map) = l;
	}
      else
#endif
	{
	  /* This function will get called to fix up the GOT entry
	     indicated by the offset on the stack, and then jump to
	     the resolved address.  */
	  got[2] = (ElfW(Addr)) &_dl_runtime_resolve;
	}
    }

  return lazy;
}

/* In elf/rtld.c _dl_start should be global so dl-start.S can reference it.  */
#define RTLD_START asm (".globl _dl_start");

#define elf_machine_type_class(type)					\
  ((((type) == R_AARCH64_JUMP_SLOT ||					\
     (type) == R_AARCH64_TLS_DTPMOD ||					\
     (type) == R_AARCH64_TLS_DTPREL ||					\
     (type) == R_AARCH64_TLS_TPREL ||					\
     (type) == R_AARCH64_TLSDESC) * ELF_RTYPE_CLASS_PLT)		\
   | (((type) == R_AARCH64_COPY) * ELF_RTYPE_CLASS_COPY))

#define ELF_MACHINE_JMP_SLOT	AARCH64_R(JUMP_SLOT)

#define DL_PLATFORM_INIT dl_platform_init ()

static inline void __attribute__ ((unused))
dl_platform_init (void)
{
  if (GLRO(dl_platform) != NULL && *GLRO(dl_platform) == '\0')
    /* Avoid an empty string which would disturb us.  */
    GLRO(dl_platform) = NULL;

#ifdef SHARED
  /* init_cpu_features has been called early from __libc_start_main in
     static executable.  */
  init_cpu_features (&GLRO(dl_aarch64_cpu_features));
#endif
}


static inline ElfW(Addr)
elf_machine_fixup_plt (struct link_map *map, lookup_t t,
		       const ElfW(Sym) *refsym, const ElfW(Sym) *sym,
		       const ElfW(Rela) *reloc,
		       ElfW(Addr) *reloc_addr,
		       ElfW(Addr) value)
{
  return *reloc_addr = value;
}

/* Return the final value of a plt relocation.  */
static inline ElfW(Addr)
elf_machine_plt_value (struct link_map *map,
		       const ElfW(Rela) *reloc,
		       ElfW(Addr) value)
{
  return value;
}

#endif

/* Names of the architecture-specific auditing callback functions.  */
#define ARCH_LA_PLTENTER aarch64_gnu_pltenter
#define ARCH_LA_PLTEXIT  aarch64_gnu_pltexit

#ifdef RESOLVE_MAP

static inline void
__attribute__ ((always_inline))
elf_machine_rela (struct link_map *map, struct r_scope_elem *scope[],
		  const ElfW(Rela) *reloc, const ElfW(Sym) *sym,
		  const struct r_found_version *version,
		  void *const reloc_addr_arg, int skip_ifunc)
{
  ElfW(Addr) *const reloc_addr = reloc_addr_arg;
  const unsigned int r_type = ELFW (R_TYPE) (reloc->r_info);

  if (__builtin_expect (r_type == AARCH64_R(RELATIVE), 0))
      *reloc_addr = map->l_addr + reloc->r_addend;
  else if (__builtin_expect (r_type == R_AARCH64_NONE, 0))
      return;
  else
    {
# ifndef RTLD_BOOTSTRAP
      const ElfW(Sym) *const refsym = sym;
# endif
      struct link_map *sym_map = RESOLVE_MAP (map, scope, &sym, version,
					      r_type);
      ElfW(Addr) value = SYMBOL_ADDRESS (sym_map, sym, true);

      if (sym != NULL
	  && __glibc_unlikely (ELFW(ST_TYPE) (sym->st_info) == STT_GNU_IFUNC)
	  && __glibc_likely (sym->st_shndx != SHN_UNDEF)
	  && __glibc_likely (!skip_ifunc))
	value = elf_ifunc_invoke (value);

      switch (r_type)
	{
	case AARCH64_R(GLOB_DAT):
	case AARCH64_R(JUMP_SLOT):
	  *reloc_addr = value + reloc->r_addend;
	  break;

# ifndef RTLD_BOOTSTRAP
	case AARCH64_R(ABS32):
#  ifdef __LP64__
	case AARCH64_R(ABS64):
#  endif
	  *reloc_addr = value + reloc->r_addend;
	  break;
	case AARCH64_R(COPY):
	  if (sym == NULL)
	      break;

	  if (sym->st_size > refsym->st_size
	      || (GLRO(dl_verbose) && sym->st_size < refsym->st_size))
	    {
	      const char *strtab;

	      strtab = (const void *) D_PTR (map, l_info[DT_STRTAB]);
	      _dl_error_printf ("\
%s: Symbol `%s' has different size in shared object, consider re-linking\n",
				RTLD_PROGNAME, strtab + refsym->st_name);
	    }
	  memcpy (reloc_addr_arg, (void *) value,
		  sym->st_size < refsym->st_size
		  ? sym->st_size : refsym->st_size);
	  break;

	case AARCH64_R(TLSDESC):
	  {
	    struct tlsdesc volatile *td =
	      (struct tlsdesc volatile *)reloc_addr;
	    if (! sym)
	      {
		td->arg = (void*)reloc->r_addend;
		td->entry = _dl_tlsdesc_undefweak;
	      }
	    else
	      {
# ifndef SHARED
		CHECK_STATIC_TLS (map, sym_map);
# else
		if (!TRY_STATIC_TLS (map, sym_map))
		  {
		    td->arg = _dl_make_tlsdesc_dynamic
		      (sym_map, sym->st_value + reloc->r_addend);
		    td->entry = _dl_tlsdesc_dynamic;
		  }
		else
# endif
		  {
		    td->arg = (void*)(sym->st_value + sym_map->l_tls_offset
				      + reloc->r_addend);
		    td->entry = _dl_tlsdesc_return;
		  }
	      }
	    break;
	  }

	case AARCH64_R(TLS_DTPMOD):
	  if (sym_map != NULL)
	    {
	      *reloc_addr = sym_map->l_tls_modid;
	    }
	  break;

	case AARCH64_R(TLS_DTPREL):
	  if (sym)
	    *reloc_addr = sym->st_value + reloc->r_addend;
	  break;

	case AARCH64_R(TLS_TPREL):
	  if (sym)
	    {
	      CHECK_STATIC_TLS (map, sym_map);
	      *reloc_addr =
		sym->st_value + reloc->r_addend + sym_map->l_tls_offset;
	    }
	  break;

	case AARCH64_R(IRELATIVE):
	  value = map->l_addr + reloc->r_addend;
	  if (__glibc_likely (!skip_ifunc))
	    value = elf_ifunc_invoke (value);
	  *reloc_addr = value;
	  break;
# endif /* !RTLD_BOOTSTRAP */

	default:
	  _dl_reloc_bad_type (map, r_type, 0);
	  break;
	}
    }
}

static inline void
__attribute__ ((always_inline))
elf_machine_rela_relative (ElfW(Addr) l_addr,
			   const ElfW(Rela) *reloc,
			   void *const reloc_addr_arg)
{
  ElfW(Addr) *const reloc_addr = reloc_addr_arg;
  *reloc_addr = l_addr + reloc->r_addend;
}

static inline void
__attribute__ ((always_inline))
elf_machine_lazy_rel (struct link_map *map, struct r_scope_elem *scope[],
		      ElfW(Addr) l_addr,
		      const ElfW(Rela) *reloc,
		      int skip_ifunc)
{
  ElfW(Addr) *const reloc_addr = (void *) (l_addr + reloc->r_offset);
  const unsigned int r_type = ELFW (R_TYPE) (reloc->r_info);
  /* Check for unexpected PLT reloc type.  */
  if (__builtin_expect (r_type == AARCH64_R(JUMP_SLOT), 1))
    {
      if (__glibc_unlikely (map->l_info[DT_AARCH64 (VARIANT_PCS)] != NULL))
	{
	  /* Check the symbol table for variant PCS symbols.  */
	  const Elf_Symndx symndx = ELFW (R_SYM) (reloc->r_info);
	  const ElfW (Sym) *symtab =
	    (const void *)D_PTR (map, l_info[DT_SYMTAB]);
	  const ElfW (Sym) *sym = &symtab[symndx];
	  if (__glibc_unlikely (sym->st_other & STO_AARCH64_VARIANT_PCS))
	    {
	      /* Avoid lazy resolution of variant PCS symbols.  */
	      const struct r_found_version *version = NULL;
	      if (map->l_info[VERSYMIDX (DT_VERSYM)] != NULL)
		{
		  const ElfW (Half) *vernum =
		    (const void *)D_PTR (map, l_info[VERSYMIDX (DT_VERSYM)]);
		  version = &map->l_versions[vernum[symndx] & 0x7fff];
		}
	      elf_machine_rela (map, scope, reloc, sym, version, reloc_addr,
				skip_ifunc);
	      return;
	    }
	}

      if (map->l_mach.plt == 0)
	*reloc_addr += l_addr;
      else
	*reloc_addr = map->l_mach.plt;
    }
  else if (__builtin_expect (r_type == AARCH64_R(TLSDESC), 1))
    {
      const Elf_Symndx symndx = ELFW (R_SYM) (reloc->r_info);
      const ElfW (Sym) *symtab = (const void *)D_PTR (map, l_info[DT_SYMTAB]);
      const ElfW (Sym) *sym = &symtab[symndx];
      const struct r_found_version *version = NULL;

      if (map->l_info[VERSYMIDX (DT_VERSYM)] != NULL)
	{
	  const ElfW (Half) *vernum =
	    (const void *)D_PTR (map, l_info[VERSYMIDX (DT_VERSYM)]);
	  version = &map->l_versions[vernum[symndx] & 0x7fff];
	}

      /* Always initialize TLS descriptors completely, because lazy
	 initialization requires synchronization at every TLS access.  */
      elf_machine_rela (map, scope, reloc, sym, version, reloc_addr,
			skip_ifunc);
    }
  else if (__glibc_unlikely (r_type == AARCH64_R(IRELATIVE)))
    {
      ElfW(Addr) value = map->l_addr + reloc->r_addend;
      if (__glibc_likely (!skip_ifunc))
	value = elf_ifunc_invoke (value);
      *reloc_addr = value;
    }
  else
    _dl_reloc_bad_type (map, r_type, 1);
}

#endif
