/* Machine-dependent ELF dynamic relocation inline functions.  OpenRISC version.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef dl_machine_h
#define dl_machine_h

#define ELF_MACHINE_NAME "or1k"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <tls.h>
#include <dl-irel.h>
#include <dl-static-tls.h>
#include <dl-machine-rel.h>

/* Return nonzero iff ELF header is compatible with the running host.  */
static inline int __attribute__ ((unused))
elf_machine_matches_host (const Elf32_Ehdr *ehdr)
{
  return ehdr->e_machine == EM_OPENRISC;
}

static inline Elf32_Addr *
or1k_get_got (void)
{
  Elf32_Addr *got;

  asm ("l.jal    0x8\n"
       " l.movhi %0, gotpchi(_GLOBAL_OFFSET_TABLE_-4)\n"
       "l.ori    %0, %0, gotpclo(_GLOBAL_OFFSET_TABLE_+0)\n"
       "l.add    %0, %0, r9\n"
       : "=r" (got) : : "r9");

  return got;
}

/* Return the link-time address of _DYNAMIC.  Conveniently, this is the
   first element of the GOT.  */
static inline Elf32_Addr
elf_machine_dynamic (void)
{
  Elf32_Addr *got = or1k_get_got ();
  return *got;
}


/* Return the run-time load address of the shared object.  */
static inline Elf32_Addr
elf_machine_load_address (void)
{
  /* Compute the difference between the runtime address of _DYNAMIC as seen
     by a GOTOFF reference, and the link-time address found in the special
     unrelocated first GOT entry.  */
  Elf32_Addr dyn;
  Elf32_Addr *got = or1k_get_got ();

  asm ("l.movhi %0, gotoffhi(_DYNAMIC);"
       "l.ori   %0, %0, gotofflo(_DYNAMIC);"
       "l.add   %0, %0, %1;"
       : "=&r"(dyn) : "r"(got));

  return dyn - *got;
}

/* Initial entry point code for the dynamic linker.  The function _dl_start
   is the real entry point; it's return value is the user program's entry
   point.

   Code is really located in dl-start.S, just tell the linker that it
   exists.  */
#define RTLD_START asm (".globl _dl_start");

/* ELF_RTYPE_CLASS_PLT iff TYPE describes relocation of a PLT entry or
   TLS variable, so undefined references should not be allowed to
   define the value.
   ELF_RTYPE_CLASS_NOCOPY iff TYPE should not be allowed to resolve to one
   of the main executable's symbols, as for a COPY reloc.  */

#define elf_machine_type_class(type) \
  (((type) == R_OR1K_JMP_SLOT \
   || (type) == R_OR1K_TLS_DTPMOD \
   || (type) == R_OR1K_TLS_DTPOFF \
   || (type) == R_OR1K_TLS_TPOFF) * ELF_RTYPE_CLASS_PLT \
   | ((type) == R_OR1K_COPY) * ELF_RTYPE_CLASS_COPY)

/* A reloc type used for ld.so cmdline arg lookups to reject PLT entries.  */
#define ELF_MACHINE_JMP_SLOT    R_OR1K_JMP_SLOT

#define ARCH_LA_PLTENTER or1k_gnu_pltenter
#define ARCH_LA_PLTEXIT or1k_gnu_pltexit

/* Set up the loaded object described by L so its unrelocated PLT
   entries will jump to the on-demand fixup code in dl-runtime.c.  */
static inline int __attribute__ ((unused, always_inline))
elf_machine_runtime_setup (struct link_map *l, struct r_scope_elem *scope[],
			   int lazy, int profile)
{
  ElfW(Addr) *pltgot;
  extern void _dl_runtime_resolve (ElfW(Word));
  extern void _dl_runtime_profile (ElfW(Word));

  if (l->l_info[DT_JMPREL] && lazy)
    {
      pltgot = (ElfW(Addr) *) D_PTR (l, l_info[DT_PLTGOT]);

      /* Fill in initial entries of the plt */

      /* Register the link_map address in the plt at pltgot[1].
	 This will also be used in the resolver for accessing the
	 link_map structure.  */
      pltgot[1] = (ElfW(Addr)) l;

      /* This function will get called to fix up the GOT entry and
	 then jump to the resolved address.  */
      pltgot[2] = (ElfW(Addr)) &_dl_runtime_resolve;

    }

  return lazy;
}

/* Mask identifying addresses reserved for the user program,
   where the dynamic linker should not map anything.  */
#define ELF_MACHINE_USER_ADDRESS_MASK   0xf8000000UL

/* We define an initialization functions.  This is called very early in
   _dl_sysdep_start.  */
#define DL_PLATFORM_INIT dl_platform_init ()

static inline void __attribute__ ((unused))
dl_platform_init (void)
{
  if (GLRO(dl_platform) != NULL && *GLRO(dl_platform) == '\0')
    /* Avoid an empty string which would disturb us.  */
    GLRO(dl_platform) = NULL;
}

static inline ElfW(Addr)
elf_machine_fixup_plt (struct link_map *map, lookup_t t,
		       const ElfW(Sym) *refsym, const ElfW(Sym) *sym,
		       const ElfW(Rela) *reloc,
		       ElfW(Addr) *reloc_addr, ElfW(Addr) value)
{
  return *reloc_addr = value;
}

/* Return the final value of a plt relocation.  */
static inline Elf32_Addr
elf_machine_plt_value (struct link_map *map, const Elf32_Rela *reloc,
		       Elf32_Addr value)
{
  return value + reloc->r_addend;
}


#endif /* !dl_machine_h */

#ifdef RESOLVE_MAP

/* Perform the relocation specified by RELOC and SYM (which is fully resolved).
   MAP is the object containing the reloc.  */

static inline void
__attribute ((always_inline))
elf_machine_rela (struct link_map *map, struct r_scope_elem *scope[],
		  const ElfW(Rela) *reloc, const ElfW(Sym) *sym,
		  const struct r_found_version *version,
		  void *const reloc_addr_arg, int skip_ifunc)
{
  Elf32_Addr *const reloc_addr = reloc_addr_arg;
  const unsigned int r_type = ELF32_R_TYPE (reloc->r_info);

  if (__glibc_unlikely (r_type == R_OR1K_NONE))
    return;
  else
    {
      const Elf32_Sym *const refsym = sym;
      struct link_map *sym_map = RESOLVE_MAP (map, scope, &sym, version,
					      r_type);
      Elf32_Addr value = SYMBOL_ADDRESS (sym_map, sym, true);

      if (sym != NULL
	  && __glibc_unlikely (ELFW(ST_TYPE) (sym->st_info) == STT_GNU_IFUNC)
	  && __glibc_likely (sym->st_shndx != SHN_UNDEF)
	  && __glibc_likely (!skip_ifunc))
	value = elf_ifunc_invoke (value);

      switch (r_type)
	{
	  case R_OR1K_COPY:
	    if (sym == NULL)
	      /* This can happen in trace mode if an object could not be
		 found.  */
	      break;
	    if (__glibc_unlikely (sym->st_size > refsym->st_size)
		|| (__glibc_unlikely (sym->st_size < refsym->st_size)
		  && GLRO(dl_verbose)))
	    {
	      const char *strtab;

	      strtab = (const char *) D_PTR (map, l_info[DT_STRTAB]);
	      _dl_error_printf ("\
%s: Symbol `%s' has different size in shared object, consider re-linking\n",
		  rtld_progname ?: "<program name unknown>",
		  strtab + refsym->st_name);
	    }
	    memcpy (reloc_addr_arg, (void *) value,
		MIN (sym->st_size, refsym->st_size));
	    break;
	  case R_OR1K_32:
	    /* Support relocations on mis-aligned offsets.  */
	    value += reloc->r_addend;
	    memcpy (reloc_addr_arg, &value, 4);
	    break;
	  case R_OR1K_GLOB_DAT:
	  case R_OR1K_JMP_SLOT:
	    *reloc_addr = value + reloc->r_addend;
	    break;
	  case R_OR1K_TLS_DTPMOD:
# ifdef RTLD_BOOTSTRAP
	    /* During startup the dynamic linker is always the module
	       with index 1.  */
	    *reloc_addr = 1;
# else
	    if (sym_map != NULL)
	      *reloc_addr = sym_map->l_tls_modid;
# endif
	    break;
	  case R_OR1K_TLS_DTPOFF:
# ifndef RTLD_BOOTSTRAP
	    *reloc_addr = (sym == NULL ? 0 : sym->st_value) + reloc->r_addend;
# endif
	    break;

	  case R_OR1K_TLS_TPOFF:
# ifdef RTLD_BOOTSTRAP
	    *reloc_addr = sym->st_value + reloc->r_addend +
	      map->l_tls_offset - TLS_TCB_SIZE;
# else
	    if (sym_map != NULL)
	      {
		CHECK_STATIC_TLS (map, sym_map);
		*reloc_addr = sym->st_value + reloc->r_addend +
		  sym_map->l_tls_offset - TLS_TCB_SIZE;
	      }
# endif
	    break;
	  default:
	    _dl_reloc_bad_type (map, r_type, 0);
	    break;
	}
    }
}

static inline void
__attribute__ ((always_inline))
elf_machine_rela_relative (Elf32_Addr l_addr, const Elf32_Rela *reloc,
			   void *const reloc_addr_arg)
{
  Elf32_Addr *const reloc_addr = reloc_addr_arg;
  *reloc_addr = l_addr + reloc->r_addend;
}

static inline void
__attribute__ ((always_inline))
elf_machine_lazy_rel (struct link_map *map, struct r_scope_elem *scope[],
		      ElfW(Addr) l_addr, const ElfW(Rela) *reloc,
		      int skip_ifunc)
{
  Elf32_Addr *const reloc_addr = (void *) (l_addr + reloc->r_offset);
  const unsigned int r_type = ELF32_R_TYPE (reloc->r_info);

  if (__glibc_likely (r_type == R_OR1K_JMP_SLOT))
      *reloc_addr += l_addr;
  else if (__glibc_unlikely (r_type == R_OR1K_NONE))
    return;
  else
    _dl_reloc_bad_type (map, r_type, 1);
}

#endif /* RESOLVE_MAP */
