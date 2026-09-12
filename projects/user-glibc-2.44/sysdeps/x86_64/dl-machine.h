/* Machine-dependent ELF dynamic relocation inline functions.  x86-64 version.
   Copyright (C) 2001-2026 Free Software Foundation, Inc.
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

#define ELF_MACHINE_NAME "x86_64"

#include <assert.h>
#include <stdint.h>
#include <sys/param.h>
#include <sysdep.h>
#include <tls.h>
#include <dl-tlsdesc.h>
#include <dl-static-tls.h>
#include <dl-machine-rel.h>
#include <isa-level.h>
#ifdef __CET__
# include <dl-cet.h>
#else
# define RTLD_START_ENABLE_X86_FEATURES
#endif

/* Translate a processor specific dynamic tag to the index in l_info array.  */
#define DT_X86_64(x) (DT_X86_64_##x - DT_LOPROC + DT_NUM)

/* Return nonzero iff ELF header is compatible with the running host.  */
static inline int __attribute__ ((unused))
elf_machine_matches_host (const ElfW(Ehdr) *ehdr)
{
  return ehdr->e_machine == EM_X86_64;
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

static inline int __attribute__ ((unused, always_inline))
elf_machine_runtime_setup (struct link_map *l, struct r_scope_elem *scope[],
			   int lazy, int profile)
{
  Elf64_Addr *got;
  extern void _dl_runtime_profile_sse (ElfW(Word)) attribute_hidden;
  extern void _dl_runtime_profile_avx (ElfW(Word)) attribute_hidden;
  extern void _dl_runtime_profile_avx512 (ElfW(Word)) attribute_hidden;

  if (l->l_info[DT_JMPREL] && lazy)
    {
      /* The GOT entries for functions in the PLT have not yet been filled
	 in.  Their initial contents will arrange when called to push an
	 offset into the .rel.plt section, push _GLOBAL_OFFSET_TABLE_[1],
	 and then jump to _GLOBAL_OFFSET_TABLE_[2].  */
      got = (Elf64_Addr *) D_PTR (l, l_info[DT_PLTGOT]);
      /* If a library is prelinked but we have to relocate anyway,
	 we have to be able to undo the prelinking of .got.plt.
	 The prelinker saved us here address of .plt + 0x16.  */
      if (got[1])
	{
	  l->l_mach.plt = got[1] + l->l_addr;
	  l->l_mach.gotplt = (ElfW(Addr)) &got[3];
	}
      /* Identify this shared object.  */
      *(ElfW(Addr) *) (got + 1) = (ElfW(Addr)) l;

#ifdef SHARED
      /* The got[2] entry contains the address of a function which gets
	 called to get the address of a so far unresolved function and
	 jump to it.  The profiling extension of the dynamic linker allows
	 to intercept the calls to collect information.  In this case we
	 don't store the address in the GOT so that all future calls also
	 end in this function.  */
      if (__glibc_unlikely (profile))
	{
	  const struct cpu_features* cpu_features = __get_cpu_features ();
	  if (X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, AVX512F))
	    *(ElfW(Addr) *) (got + 2) = (ElfW(Addr)) &_dl_runtime_profile_avx512;
	  else if (X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, AVX))
	    *(ElfW(Addr) *) (got + 2) = (ElfW(Addr)) &_dl_runtime_profile_avx;
	  else
	    *(ElfW(Addr) *) (got + 2) = (ElfW(Addr)) &_dl_runtime_profile_sse;

	  if (GLRO(dl_profile) != NULL
	      && _dl_name_match_p (GLRO(dl_profile), l))
	    /* This is the object we are looking for.  Say that we really
	       want profiling and the timers are started.  */
	    GL(dl_profile_map) = l;
	}
      else
#endif
	{
	  /* This function will get called to fix up the GOT entry
	     indicated by the offset on the stack, and then jump to
	     the resolved address.  */
	  *(ElfW(Addr) *) (got + 2)
	    = (ElfW(Addr)) GLRO(dl_x86_64_runtime_resolve);
	}
    }

  return lazy;
}

/* Initial entry point code for the dynamic linker.
   The C function `_dl_start' is the real entry point;
   its return value is the user program's entry point.  */
#define RTLD_START asm ("\n\
.text\n\
	.align 16\n\
.globl _start\n\
.globl _dl_start_user\n\
_start:\n\
	mov %" RSP_LP ", %" RDI_LP "\n\
	call _dl_start\n\
_dl_start_user:\n\
	# Save the user entry point address in %r12.\n\
	mov %" RAX_LP ", %" R12_LP "\n\
	# Save %rsp value in %r13.\n\
	mov %" RSP_LP ", % " R13_LP "\n\
"\
	RTLD_START_ENABLE_X86_FEATURES \
"\
	# Read the original argument count.\n\
	mov (%rsp), %" RDX_LP "\n\
	# Call _dl_init (struct link_map *main_map, int argc, char **argv, char **env)\n\
	# argc -> rsi\n\
	mov %" RDX_LP ", %" RSI_LP "\n\
	# And align stack for the _dl_init call. \n\
	and $-16, %" RSP_LP "\n\
	# _dl_loaded -> rdi\n\
	mov _rtld_local(%rip), %" RDI_LP "\n\
	# env -> rcx\n\
	lea 2*" LP_SIZE "(%r13,%rdx," LP_SIZE "), %" RCX_LP "\n\
	# argv -> rdx\n\
	lea " LP_SIZE "(%r13), %" RDX_LP "\n\
	# Clear %rbp to mark outermost frame obviously even for constructors.\n\
	xorl %ebp, %ebp\n\
	# Call the function to run the initializers.\n\
	call _dl_init\n\
	# Pass our finalizer function to the user in %rdx, as per ELF ABI.\n\
	lea _dl_fini(%rip), %" RDX_LP "\n\
	# And make sure %rsp points to argc stored on the stack.\n\
	mov %" R13_LP ", %" RSP_LP "\n\
	# Jump to the user's entry point.\n\
	jmp *%r12\n\
.previous\n\
");

/* ELF_RTYPE_CLASS_PLT iff TYPE describes relocation of a PLT entry or
   TLS variable, so undefined references should not be allowed to
   define the value.
   ELF_RTYPE_CLASS_COPY iff TYPE should not be allowed to resolve to one
   of the main executable's symbols, as for a COPY reloc.  */
#define elf_machine_type_class(type)					      \
  ((((type) == R_X86_64_JUMP_SLOT					      \
     || (type) == R_X86_64_DTPMOD64					      \
     || (type) == R_X86_64_DTPOFF64					      \
     || (type) == R_X86_64_TPOFF64					      \
     || (type) == R_X86_64_TLSDESC)					      \
    * ELF_RTYPE_CLASS_PLT)						      \
   | (((type) == R_X86_64_COPY) * ELF_RTYPE_CLASS_COPY))

/* A reloc type used for ld.so cmdline arg lookups to reject PLT entries.  */
#define ELF_MACHINE_JMP_SLOT	R_X86_64_JUMP_SLOT

/* The relative ifunc relocation.  */
// XXX This is a work-around for a broken linker.  Remove!
#define ELF_MACHINE_IRELATIVE	R_X86_64_IRELATIVE

/* We define an initialization function.  This is called very early in
   _dl_sysdep_start.  */
#define DL_PLATFORM_INIT dl_platform_init ()

static inline void __attribute__ ((unused))
dl_platform_init (void)
{
#if IS_IN (rtld)
  /* _dl_x86_init_cpu_features is a wrapper for init_cpu_features which
     has been called early from __libc_start_main in static executable.  */
  _dl_x86_init_cpu_features ();
#else
  if (GLRO(dl_platform) != NULL && *GLRO(dl_platform) == '\0')
    /* Avoid an empty string which would disturb us.  */
    GLRO(dl_platform) = NULL;
#endif
}

static inline ElfW(Addr)
elf_machine_fixup_plt (struct link_map *map, lookup_t t,
		       const ElfW(Sym) *refsym, const ElfW(Sym) *sym,
		       const ElfW(Rela) *reloc,
		       ElfW(Addr) *reloc_addr, ElfW(Addr) value)
{
  return *reloc_addr = value;
}

/* Return the final value of a PLT relocation.  On x86-64 the
   JUMP_SLOT relocation ignores the addend.  */
static inline ElfW(Addr)
elf_machine_plt_value (struct link_map *map, const ElfW(Rela) *reloc,
		       ElfW(Addr) value)
{
  return value;
}


/* Names of the architecture-specific auditing callback functions.  */
#ifdef __LP64__
#define ARCH_LA_PLTENTER x86_64_gnu_pltenter
#define ARCH_LA_PLTEXIT x86_64_gnu_pltexit
#else
#define ARCH_LA_PLTENTER x32_gnu_pltenter
#define ARCH_LA_PLTEXIT x32_gnu_pltexit
#endif

#endif /* !dl_machine_h */

#ifdef RESOLVE_MAP

/* Perform the relocation specified by RELOC and SYM (which is fully resolved).
   MAP is the object containing the reloc.  */

static inline void __attribute__((always_inline))
elf_machine_rela (struct link_map *map, struct r_scope_elem *scope[],
		  const ElfW(Rela) *reloc, const ElfW(Sym) *sym,
		  const struct r_found_version *version,
		  void *const reloc_addr_arg, int skip_ifunc)
{
  ElfW(Addr) *const reloc_addr = reloc_addr_arg;
  const unsigned long int r_type = ELFW(R_TYPE) (reloc->r_info);

# if !defined RTLD_BOOTSTRAP
  if (__glibc_unlikely (r_type == R_X86_64_RELATIVE))
    *reloc_addr = map->l_addr + reloc->r_addend;
  else
# endif
# if !defined RTLD_BOOTSTRAP
  /* l_addr + r_addend may be > 0xffffffff and R_X86_64_RELATIVE64
     relocation updates the whole 64-bit entry.  */
  if (__glibc_unlikely (r_type == R_X86_64_RELATIVE64))
    *(Elf64_Addr *) reloc_addr = (Elf64_Addr) map->l_addr + reloc->r_addend;
  else
# endif
  if (__glibc_unlikely (r_type == R_X86_64_NONE))
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
	{
# ifndef RTLD_BOOTSTRAP
	  if (sym_map != map
	      && !sym_map->l_relocated)
	    {
	      const char *strtab
		= (const char *) D_PTR (map, l_info[DT_STRTAB]);
	      if (sym_map->l_type == lt_executable)
		_dl_fatal_printf ("\
%s: IFUNC symbol '%s' referenced in '%s' is defined in the executable \
and creates an unsatisfiable circular dependency.\n",
				  RTLD_PROGNAME, strtab + refsym->st_name,
				  map->l_name);
	      else
		_dl_error_printf ("\
%s: Relink `%s' with `%s' for IFUNC symbol `%s'\n",
				  RTLD_PROGNAME, map->l_name,
				  sym_map->l_name,
				  strtab + refsym->st_name);
	    }
# endif
	  value = ((ElfW(Addr) (*) (void)) value) ();
	}

      switch (r_type)
	{
	case R_X86_64_JUMP_SLOT:
	  map->l_has_jump_slot_reloc = true;
	  [[fallthrough]];
	case R_X86_64_GLOB_DAT:
	  *reloc_addr = value;
	  break;

# ifndef RTLD_BOOTSTRAP
#  ifdef __ILP32__
	case R_X86_64_SIZE64:
	  /* Set to symbol size plus addend.  */
	  *(Elf64_Addr *) (uintptr_t) reloc_addr
	    = (Elf64_Addr) sym->st_size + reloc->r_addend;
	  break;

	case R_X86_64_SIZE32:
#  else
	case R_X86_64_SIZE64:
#  endif
	  /* Set to symbol size plus addend.  */
	  value = sym->st_size;
	  *reloc_addr = value + reloc->r_addend;
	  break;

	case R_X86_64_DTPMOD64:
	  /* Get the information from the link map returned by the
	     resolve function.  */
	  if (sym_map != NULL)
	    *reloc_addr = sym_map->l_tls_modid;
	  break;
	case R_X86_64_DTPOFF64:
	  /* During relocation all TLS symbols are defined and used.
	     Therefore the offset is already correct.  */
	  if (sym != NULL)
	    {
	      value = sym->st_value + reloc->r_addend;
#  ifdef __ILP32__
	      /* This relocation type computes a signed offset that is
		 usually negative.  The symbol and addend values are 32
		 bits but the GOT entry is 64 bits wide and the whole
		 64-bit entry is used as a signed quantity, so we need
		 to sign-extend the computed value to 64 bits.  */
	      *(Elf64_Sxword *) reloc_addr = (Elf64_Sxword) (Elf32_Sword) value;
#  else
	      *reloc_addr = value;
#  endif
	    }
	  break;
	case R_X86_64_TLSDESC:
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
#  ifndef SHARED
		CHECK_STATIC_TLS (map, sym_map);
#  else
		if (!TRY_STATIC_TLS (map, sym_map))
		  {
		    td->arg = _dl_make_tlsdesc_dynamic
		      (sym_map, sym->st_value + reloc->r_addend);
		    td->entry = GLRO(dl_x86_tlsdesc_dynamic);
		  }
		else
#  endif
		  {
		    td->arg = (void*)(sym->st_value - sym_map->l_tls_offset
				      + reloc->r_addend);
		    td->entry = _dl_tlsdesc_return;
		  }
	      }
	    break;
	  }
	case R_X86_64_TPOFF64:
	  /* The offset is negative, forward from the thread pointer.  */
	  if (sym != NULL)
	    {
	      CHECK_STATIC_TLS (map, sym_map);
	      /* We know the offset of the object the symbol is contained in.
		 It is a negative value which will be added to the
		 thread pointer.  */
	      value = (sym->st_value + reloc->r_addend
		       - sym_map->l_tls_offset);
# ifdef __ILP32__
	      /* The symbol and addend values are 32 bits but the GOT
		 entry is 64 bits wide and the whole 64-bit entry is used
		 as a signed quantity, so we need to sign-extend the
		 computed value to 64 bits.  */
	      *(Elf64_Sxword *) reloc_addr = (Elf64_Sxword) (Elf32_Sword) value;
# else
	      *reloc_addr = value;
# endif
	    }
	  break;

	case R_X86_64_64:
	  /* value + r_addend may be > 0xffffffff and R_X86_64_64
	     relocation updates the whole 64-bit entry.  */
	  *(Elf64_Addr *) reloc_addr = (Elf64_Addr) value + reloc->r_addend;
	  break;
#  ifndef __ILP32__
	case R_X86_64_SIZE32:
	  /* Set to symbol size plus addend.  */
	  value = sym->st_size;
#  endif
	  [[fallthrough]];
	case R_X86_64_32:
	  value += reloc->r_addend;
	  *(unsigned int *) reloc_addr = value;

	  const char *fmt;
	  if (__glibc_unlikely (value > UINT_MAX))
	    {
	      const char *strtab;

	      fmt = "\
%s: Symbol `%s' causes overflow in R_X86_64_32 relocation\n";
	    print_err:
	      strtab = (const char *) D_PTR (map, l_info[DT_STRTAB]);

	      _dl_error_printf (fmt, RTLD_PROGNAME, strtab + refsym->st_name);
	    }
	  break;
	  /* Not needed for dl-conflict.c.  */
	case R_X86_64_PC32:
	  value += reloc->r_addend - (ElfW(Addr)) reloc_addr;
	  *(unsigned int *) reloc_addr = value;
	  if (__glibc_unlikely (value != (int) value))
	    {
	      fmt = "\
%s: Symbol `%s' causes overflow in R_X86_64_PC32 relocation\n";
	      goto print_err;
	    }
	  break;
	case R_X86_64_COPY:
	  if (sym == NULL)
	    /* This can happen in trace mode if an object could not be
	       found.  */
	    break;
	  memcpy (reloc_addr_arg, (void *) value,
		  MIN (sym->st_size, refsym->st_size));
	  if (__glibc_unlikely (sym->st_size > refsym->st_size)
	      || (__glibc_unlikely (sym->st_size < refsym->st_size)
		  && GLRO(dl_verbose)))
	    {
	      fmt = "\
%s: Symbol `%s' has different size in shared object, consider re-linking\n";
	      goto print_err;
	    }
	  break;
	case R_X86_64_IRELATIVE:
	  value = map->l_addr + reloc->r_addend;
	  if (__glibc_likely (!skip_ifunc))
	    value = ((ElfW(Addr) (*) (void)) value) ();
	  *reloc_addr = value;
	  break;
	default:
	  _dl_reloc_bad_type (map, r_type, 0);
	  break;
# endif /* !RTLD_BOOTSTRAP */
	}
    }
}

static inline void
__attribute ((always_inline))
elf_machine_rela_relative (ElfW(Addr) l_addr, const ElfW(Rela) *reloc,
			   void *const reloc_addr_arg)
{
  ElfW(Addr) *const reloc_addr = reloc_addr_arg;
#if !defined RTLD_BOOTSTRAP
  /* l_addr + r_addend may be > 0xffffffff and R_X86_64_RELATIVE64
     relocation updates the whole 64-bit entry.  */
  if (__glibc_unlikely (ELFW(R_TYPE) (reloc->r_info) == R_X86_64_RELATIVE64))
    *(Elf64_Addr *) reloc_addr = (Elf64_Addr) l_addr + reloc->r_addend;
  else
#endif
    {
      assert (ELFW(R_TYPE) (reloc->r_info) == R_X86_64_RELATIVE);
      *reloc_addr = l_addr + reloc->r_addend;
    }
}

static inline void
__attribute ((always_inline))
elf_machine_lazy_rel (struct link_map *map, struct r_scope_elem *scope[],
		      ElfW(Addr) l_addr, const ElfW(Rela) *reloc,
		      int skip_ifunc)
{
  ElfW(Addr) *const reloc_addr = (void *) (l_addr + reloc->r_offset);
  const unsigned long int r_type = ELFW(R_TYPE) (reloc->r_info);

  /* Check for unexpected PLT reloc type.  */
  if (__glibc_likely (r_type == R_X86_64_JUMP_SLOT))
    {
      /* Prelink has been deprecated.  */
      if (__glibc_likely (map->l_mach.plt == 0))
	*reloc_addr += l_addr;
      else
	*reloc_addr =
	  map->l_mach.plt
	  + (((ElfW(Addr)) reloc_addr) - map->l_mach.gotplt) * 2;
    }
  else if (__glibc_likely (r_type == R_X86_64_TLSDESC))
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

      /* Always initialize TLS descriptors completely at load time, in
	 case static TLS is allocated for it that requires locking.  */
      elf_machine_rela (map, scope, reloc, sym, version, reloc_addr, skip_ifunc);
    }
  else if (__glibc_unlikely (r_type == R_X86_64_IRELATIVE))
    {
      ElfW(Addr) value = map->l_addr + reloc->r_addend;
      if (__glibc_likely (!skip_ifunc))
	value = ((ElfW(Addr) (*) (void)) value) ();
      *reloc_addr = value;
    }
  else
    _dl_reloc_bad_type (map, r_type, 1);
}

#endif /* RESOLVE_MAP */

#if !defined ELF_DYNAMIC_AFTER_RELOC && !defined RTLD_BOOTSTRAP \
    && defined SHARED
# define ELF_DYNAMIC_AFTER_RELOC(map, lazy) \
  x86_64_dynamic_after_reloc (map, (lazy))

# define JMP32_INSN_OPCODE	0xe9
# define JMP32_INSN_SIZE	5
# define JMPABS_INSN_OPCODE	0xa100d5
# define JMPABS_INSN_SIZE	11
# define INT3_INSN_OPCODE	0xcc

static const char *
x86_64_reloc_symbol_name (struct link_map *map, const ElfW(Rela) *reloc)
{
  const ElfW(Sym) *const symtab
    = (const void *) map->l_info[DT_SYMTAB]->d_un.d_ptr;
  const ElfW(Sym) *const refsym = &symtab[ELFW (R_SYM) (reloc->r_info)];
  const char *strtab = (const char *) map->l_info[DT_STRTAB]->d_un.d_ptr;
  return strtab + refsym->st_name;
}

static void
x86_64_rewrite_plt (struct link_map *map, ElfW(Addr) plt_rewrite)
{
  ElfW(Addr) l_addr = map->l_addr;
  ElfW(Addr) pltent = map->l_info[DT_X86_64 (PLTENT)]->d_un.d_val;
  ElfW(Addr) start = map->l_info[DT_JMPREL]->d_un.d_ptr;
  ElfW(Addr) size = map->l_info[DT_PLTRELSZ]->d_un.d_val;
  const ElfW(Rela) *reloc = (const void *) start;
  const ElfW(Rela) *reloc_end = (const void *) (start + size);

# ifdef __CET__
  bool ibt_enabled_p = dl_cet_ibt_enabled ();
# else
  bool ibt_enabled_p = false;
# endif

  if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))
    _dl_debug_printf ("\nchanging PLT in '%s' to direct branch\n",
		      DSO_FILENAME (map->l_name));

  for (; reloc < reloc_end; reloc++)
    if (ELFW(R_TYPE) (reloc->r_info) == R_X86_64_JUMP_SLOT)
      {
	/* Get the value from the GOT entry.  */
	ElfW(Addr) value = *(ElfW(Addr) *) (l_addr + reloc->r_offset);

	/* Get the corresponding PLT entry from r_addend.  */
	ElfW(Addr) branch_start = l_addr + reloc->r_addend;
	/* Skip ENDBR64 if IBT isn't enabled.  */
	if (!ibt_enabled_p)
	  branch_start = ALIGN_DOWN (branch_start, pltent);
	/* Get the displacement from the branch target.  NB: We must use
	   64-bit integer on x32 to avoid overflow.  */
	uint64_t disp = (uint64_t) value - branch_start - JMP32_INSN_SIZE;
	ElfW(Addr) plt_end;
	ElfW(Addr) pad;

	plt_end = (branch_start | (pltent - 1)) + 1;

	/* Update the PLT entry.  */
	if (((uint64_t) disp + (uint64_t) ((uint32_t) INT32_MIN))
	    <= (uint64_t) UINT32_MAX)
	  {
	    pad = branch_start + JMP32_INSN_SIZE;

	    if (__glibc_unlikely (pad > plt_end))
	      continue;

	    /* If the target branch can be reached with a direct branch,
	       rewrite the PLT entry with a direct branch.  */
	    if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_BINDINGS))
	      {
		const char *sym_name = x86_64_reloc_symbol_name (map,
								 reloc);
		_dl_debug_printf ("changing '%s' PLT entry in '%s' to "
				  "direct branch\n", sym_name,
				  DSO_FILENAME (map->l_name));
	      }

	    /* Write out direct branch.  */
	    *(uint8_t *) branch_start = JMP32_INSN_OPCODE;
	    *(uint32_t *) (branch_start + 1) = disp;
	  }
	else
	  {
	    if (GL(dl_x86_feature_control).plt_rewrite
		!= plt_rewrite_jmpabs)
	      {
		if (__glibc_unlikely (GLRO(dl_debug_mask)
				      & DL_DEBUG_BINDINGS))
		  {
		    const char *sym_name
		      = x86_64_reloc_symbol_name (map, reloc);
		    _dl_debug_printf ("skipping '%s' PLT entry in '%s'\n",
				      sym_name,
				      DSO_FILENAME (map->l_name));
		  }
		continue;
	      }

	    pad = branch_start + JMPABS_INSN_SIZE;

	    if (__glibc_unlikely (pad > plt_end))
	      continue;

	    /* Rewrite the PLT entry with JMPABS.  */
	    if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_BINDINGS))
	      {
		const char *sym_name = x86_64_reloc_symbol_name (map,
								 reloc);
		_dl_debug_printf ("changing '%s' PLT entry in '%s' to "
				  "JMPABS\n", sym_name,
				  DSO_FILENAME (map->l_name));
	      }

	    /* "jmpabs $target" for 64-bit displacement.  NB: JMPABS has
	       a 3-byte opcode + 64bit address.  There is a 1-byte overlap
	       between 4-byte write and 8-byte write.  */
	    *(uint32_t *) (branch_start) = JMPABS_INSN_OPCODE;
	    *(uint64_t *) (branch_start + 3) = value;
	  }

	/* Fill the unused part of the PLT entry with INT3.  */
	for (; pad < plt_end; pad++)
	  *(uint8_t *) pad = INT3_INSN_OPCODE;
      }
}

static inline void
x86_64_rewrite_plt_in_place (struct link_map *map)
{
  /* Adjust DT_X86_64_PLT address and DT_X86_64_PLTSZ values.  */
  ElfW(Addr) plt = (map->l_info[DT_X86_64 (PLT)]->d_un.d_ptr
		    + map->l_addr);
  size_t pagesize = GLRO(dl_pagesize);
  ElfW(Addr) plt_aligned = ALIGN_DOWN (plt, pagesize);
  size_t pltsz = (map->l_info[DT_X86_64 (PLTSZ)]->d_un.d_val
		  + plt - plt_aligned);

  if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))
    _dl_debug_printf ("\nchanging PLT in '%s' to writable\n",
		      DSO_FILENAME (map->l_name));

  if (__glibc_unlikely (__mprotect ((void *) plt_aligned, pltsz,
				    PROT_WRITE | PROT_READ) < 0))
    {
      if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))
	_dl_debug_printf ("\nfailed to change PLT in '%s' to writable\n",
			  DSO_FILENAME (map->l_name));
      return;
    }

  x86_64_rewrite_plt (map, plt_aligned);

  if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_FILES))
    _dl_debug_printf ("\nchanging PLT in '%s' back to read-only\n",
		      DSO_FILENAME (map->l_name));

  if (__glibc_unlikely (__mprotect ((void *) plt_aligned, pltsz,
				    PROT_EXEC | PROT_READ) < 0))
    _dl_signal_error (0, DSO_FILENAME (map->l_name), NULL,
		      "failed to change PLT back to read-only");
}

/* Rewrite PLT entries to direct branch if possible.  */

static inline void
x86_64_dynamic_after_reloc (struct link_map *map, int lazy)
{
  /* Ignore DT_X86_64_PLT if the lazy binding is enabled.  */
  if (lazy != 0)
    return;

  /* Ignore DT_X86_64_PLT if PLT rewrite isn't enabled.  */
  if (__glibc_likely (GL(dl_x86_feature_control).plt_rewrite
		      == plt_rewrite_none))
    return;

  if (__glibc_likely (map->l_info[DT_X86_64 (PLT)] == NULL))
    return;

  /* Ignore DT_X86_64_PLT if there is no R_X86_64_JUMP_SLOT.  */
  if (map->l_has_jump_slot_reloc == 0)
    return;

  /* Ignore DT_X86_64_PLT if
     1. DT_JMPREL isn't available or its value is 0.
     2. DT_PLTRELSZ is 0.
     3. DT_X86_64_PLTENT isn't available or its value is smaller than
	16 bytes.
     4. DT_X86_64_PLTSZ isn't available or its value is smaller than
	DT_X86_64_PLTENT's value or isn't a multiple of DT_X86_64_PLTENT's
	value.  */
  if (map->l_info[DT_JMPREL] == NULL
      || map->l_info[DT_JMPREL]->d_un.d_ptr == 0
      || map->l_info[DT_PLTRELSZ]->d_un.d_val == 0
      || map->l_info[DT_X86_64 (PLTSZ)] == NULL
      || map->l_info[DT_X86_64 (PLTENT)] == NULL
      || map->l_info[DT_X86_64 (PLTENT)]->d_un.d_val < 16
      || (map->l_info[DT_X86_64 (PLTSZ)]->d_un.d_val
	  < map->l_info[DT_X86_64 (PLTENT)]->d_un.d_val)
      || (map->l_info[DT_X86_64 (PLTSZ)]->d_un.d_val
	  % map->l_info[DT_X86_64 (PLTENT)]->d_un.d_val) != 0)
    return;

  x86_64_rewrite_plt_in_place (map);
}
#endif
