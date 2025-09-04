/* Machine-dependent ELF dynamic relocation inline functions.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef dl_machine_h
#define dl_machine_h

#define ELF_MACHINE_NAME "LoongArch"

#include <entry.h>
#include <elf/elf.h>
#include <sys/asm.h>
#include <dl-tlsdesc.h>
#include <dl-static-tls.h>
#include <dl-machine-rel.h>

#include <cpu-features.c>

#ifndef _RTLD_PROLOGUE
# define _RTLD_PROLOGUE(entry)					\
	".globl\t" __STRING (entry) "\n\t"			\
	".type\t" __STRING (entry) ", @function\n\t"		\
	CFI_STARTPROC "\n"					\
	__STRING (entry) ":\n"
#endif

#ifndef _RTLD_EPILOGUE
# define _RTLD_EPILOGUE(entry)					\
	CFI_ENDPROC "\n\t"					\
	".size\t" __STRING (entry) ", . - " __STRING (entry) "\n"
#endif

#define ELF_MACHINE_JMP_SLOT R_LARCH_JUMP_SLOT
#define ELF_MACHINE_IRELATIVE R_LARCH_IRELATIVE

#define elf_machine_type_class(type)				\
  ((ELF_RTYPE_CLASS_PLT *((type) == ELF_MACHINE_JMP_SLOT))	\
   | (ELF_RTYPE_CLASS_COPY *((type) == R_LARCH_COPY)))

#define ELF_MACHINE_NO_REL 1
#define ELF_MACHINE_NO_RELA 0

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
  init_cpu_features (&GLRO(dl_larch_cpu_features));
#endif
}


/* Return nonzero iff ELF header is compatible with the running host.  */
static inline int
elf_machine_matches_host (const ElfW (Ehdr) *ehdr)
{
  /* We can only run LoongArch binaries.  */
  if (ehdr->e_machine != EM_LOONGARCH)
    return 0;

  return 1;
}

/* Return the run-time load address of the shared object.  */
static inline ElfW (Addr) elf_machine_load_address (void)
{
  extern const ElfW(Ehdr) __ehdr_start attribute_hidden;
  return (ElfW(Addr)) &__ehdr_start;
}

/* Return the link-time address of _DYNAMIC.  */
static inline ElfW (Addr) elf_machine_dynamic (void)
{
  extern ElfW(Dyn) _DYNAMIC[] attribute_hidden;
  return (ElfW(Addr)) _DYNAMIC - elf_machine_load_address ();
}

/* Initial entry point code for the dynamic linker.
   The C function `_dl_start' is the real entry point;
   its return value is the user program's entry point.  */

#define RTLD_START asm (\
	".text\n\
	" _RTLD_PROLOGUE (ENTRY_POINT) "\
	.cfi_label .Ldummy   \n\
	" CFI_UNDEFINED (1) "   \n\
	or	$a0, $sp, $zero   \n\
	bl	_dl_start   \n\
	# Stash user entry point in s0.   \n\
	or	$s0, $a0, $zero   \n\
	# Load the original argument count.   \n\
	ld.d	$a1, $sp, 0   \n\
	# Call _dl_init (struct link_map *main_map, int argc, \
			 char **argv, char **env)    \n\
	la	$a0, _rtld_local   \n\
	ld.d	$a0, $a0, 0   \n\
	addi.d	$a2, $sp, 8   \n\
	slli.d	$a3, $a1, 3   \n\
	add.d	$a3, $a3, $a2   \n\
	addi.d	$a3, $a3, 8   \n\
	# Stash the stack pointer in s1.\n\
	or	$s1, $sp, $zero	\n\
	# Adjust $sp for 16-aligned   \n\
	bstrins.d	$sp, $zero, 3, 0  \n\
	# Call the function to run the initializers.   \n\
	bl	_dl_init   \n\
	# Restore the stack pointer for _start.\n\
	or	$sp, $s1, $zero	 \n\
	# Pass our finalizer function to _start.   \n\
	la	$a0, _dl_fini   \n\
	# Jump to the user entry point.   \n\
	jirl	$zero, $s0, 0   \n\
	" _RTLD_EPILOGUE (ENTRY_POINT) "\
	.previous");

/* Names of the architecture-specific auditing callback functions.  */
#define ARCH_LA_PLTENTER loongarch_gnu_pltenter
#define ARCH_LA_PLTEXIT loongarch_gnu_pltexit

/* Bias .got.plt entry by the offset requested by the PLT header.  */
#define elf_machine_plt_value(map, reloc, value) (value)

static inline ElfW (Addr)
elf_machine_fixup_plt (struct link_map *map, lookup_t t,
			 const ElfW (Sym) *refsym, const ElfW (Sym) *sym,
			 const ElfW (Rela) *reloc, ElfW (Addr) *reloc_addr,
			 ElfW (Addr) value)
{
  return *reloc_addr = value;
}

#endif /* !dl_machine_h */

#ifdef RESOLVE_MAP

/* Perform a relocation described by R_INFO at the location pointed to
   by RELOC_ADDR.  SYM is the relocation symbol specified by R_INFO and
   MAP is the object containing the reloc.  */

static inline void __attribute__ ((always_inline))
elf_machine_rela (struct link_map *map, struct r_scope_elem *scope[],
		  const ElfW (Rela) *reloc,
		  const ElfW (Sym) *sym,
		  const struct r_found_version *version,
		  void *const reloc_addr, int skip_ifunc)
{
  ElfW (Addr) r_info = reloc->r_info;
  const unsigned long int r_type = ELFW (R_TYPE) (r_info);
  ElfW (Addr) *addr_field = (ElfW (Addr) *) reloc_addr;
  const ElfW (Sym) *const __attribute__ ((unused)) refsym = sym;
  struct link_map *sym_map = RESOLVE_MAP (map, scope, &sym, version, r_type);
  ElfW (Addr) value = 0;
  if (sym_map != NULL)
    value = SYMBOL_ADDRESS (sym_map, sym, true) + reloc->r_addend;

  if (sym != NULL
      && __glibc_unlikely (ELFW (ST_TYPE) (sym->st_info) == STT_GNU_IFUNC)
      && __glibc_likely (sym->st_shndx != SHN_UNDEF)
      && __glibc_likely (!skip_ifunc))
    value = ((ElfW (Addr) (*) (int)) value) (GLRO (dl_hwcap));

  switch (r_type)
    {

    case R_LARCH_JUMP_SLOT:
    case __WORDSIZE == 64 ? R_LARCH_64 : R_LARCH_32:
      *addr_field = value;
      break;

    case R_LARCH_NONE:
      break;

#ifndef RTLD_BOOTSTRAP
    case __WORDSIZE == 64 ? R_LARCH_TLS_DTPMOD64 : R_LARCH_TLS_DTPMOD32:
      *addr_field = sym_map->l_tls_modid;
      break;

    case __WORDSIZE == 64 ? R_LARCH_TLS_DTPREL64 : R_LARCH_TLS_DTPREL32:
      *addr_field = TLS_DTPREL_VALUE (sym) + reloc->r_addend;
      break;

    case __WORDSIZE == 64 ? R_LARCH_TLS_TPREL64 : R_LARCH_TLS_TPREL32:
      CHECK_STATIC_TLS (map, sym_map);
      *addr_field = TLS_TPREL_VALUE (sym_map, sym) + reloc->r_addend;
      break;

    case __WORDSIZE == 64 ? R_LARCH_TLS_DESC64 : R_LARCH_TLS_DESC32:
      {
	struct tlsdesc volatile *td = (struct tlsdesc volatile *)addr_field;
	if (sym == NULL)
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
		td->arg = _dl_make_tlsdesc_dynamic (sym_map,
			      sym->st_value + reloc->r_addend);
		td->entry = _dl_tlsdesc_dynamic;
	      }
	    else
# endif
	      {
		td->arg = (void *)(TLS_TPREL_VALUE (sym_map, sym)
			    + reloc->r_addend);
		td->entry = _dl_tlsdesc_return;
	      }
	  }
	break;
      }

    case R_LARCH_COPY:
      {
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
	  memcpy (reloc_addr, (void *) value,
		  MIN (sym->st_size, refsym->st_size));
	    break;
      }

    case R_LARCH_RELATIVE:
      *addr_field = map->l_addr + reloc->r_addend;
      break;

    case R_LARCH_IRELATIVE:
      value = map->l_addr + reloc->r_addend;
      if (__glibc_likely (!skip_ifunc))
	value = ((ElfW (Addr) (*) (void)) value) ();
      *addr_field = value;
      break;

#endif

    default:
      _dl_reloc_bad_type (map, r_type, 0);
      break;
    }
}

static inline void __attribute__ ((always_inline))
elf_machine_rela_relative (ElfW (Addr) l_addr, const ElfW (Rela) *reloc,
			   void *const reloc_addr)
{
  *(ElfW (Addr) *) reloc_addr = l_addr + reloc->r_addend;
}

static inline void __attribute__ ((always_inline))
elf_machine_lazy_rel (struct link_map *map, struct r_scope_elem *scope[],
		      ElfW (Addr) l_addr,
		      const ElfW (Rela) *reloc, int skip_ifunc)
{
  ElfW (Addr) *const reloc_addr = (void *) (l_addr + reloc->r_offset);
  const unsigned int r_type = ELFW (R_TYPE) (reloc->r_info);

  /* Check for unexpected PLT reloc type.  */
  if (__glibc_likely (r_type == R_LARCH_JUMP_SLOT))
    {
      if (__glibc_unlikely (map->l_mach.plt == 0))
	{
	  if (l_addr)
	    *reloc_addr += l_addr;
	}
      else
	*reloc_addr = map->l_mach.plt;
    }
  else if (__glibc_likely (r_type == R_LARCH_TLS_DESC64)
	    || __glibc_likely (r_type == R_LARCH_TLS_DESC32))
    {
      const Elf_Symndx symndx = ELFW (R_SYM) (reloc->r_info);
      const ElfW (Sym) *symtab = (const void *)D_PTR (map, l_info[DT_SYMTAB]);
      const ElfW (Sym) *sym = &symtab[symndx];
      const struct r_found_version *version = NULL;

      if (map->l_info[VERSYMIDX (DT_VERSYM)] != NULL)
	{
	  const ElfW (Half) *vernum = (const void *)D_PTR (map,
					  l_info[VERSYMIDX (DT_VERSYM)]);
	  version = &map->l_versions[vernum[symndx] & 0x7fff];
	}

      /* Always initialize TLS descriptors completely, because lazy
	 initialization requires synchronization at every TLS access.  */
      elf_machine_rela (map, scope, reloc, sym, version, reloc_addr,
			skip_ifunc);
    }
  else
    _dl_reloc_bad_type (map, r_type, 1);
}

/* Set up the loaded object described by L so its stub function
   will jump to the on-demand fixup code __dl_runtime_resolve.  */

static inline int __attribute__ ((always_inline))
elf_machine_runtime_setup (struct link_map *l, struct r_scope_elem *scope[],
			   int lazy, int profile)
{
#ifndef RTLD_BOOTSTRAP
  /* If using PLTs, fill in the first two entries of .got.plt.  */
  if (l->l_info[DT_JMPREL])
    {
#if !defined __loongarch_soft_float
      extern void _dl_runtime_resolve_lasx (void) attribute_hidden;
      extern void _dl_runtime_resolve_lsx (void) attribute_hidden;
      extern void _dl_runtime_profile_lasx (void) attribute_hidden;
      extern void _dl_runtime_profile_lsx (void) attribute_hidden;
#endif
      extern void _dl_runtime_resolve (void) attribute_hidden;
      extern void _dl_runtime_profile (void) attribute_hidden;

      ElfW (Addr) *gotplt = (ElfW (Addr) *) D_PTR (l, l_info[DT_PLTGOT]);

      /* The got[0] entry contains the address of a function which gets
	 called to get the address of a so far unresolved function and
	 jump to it.  The profiling extension of the dynamic linker allows
	 to intercept the calls to collect information.  In this case we
	 don't store the address in the GOT so that all future calls also
	 end in this function.  */
#ifdef SHARED
      if (profile != 0)
	{
#if !defined __loongarch_soft_float
	  if (RTLD_SUPPORT_LASX)
	    gotplt[0] = (ElfW(Addr)) &_dl_runtime_profile_lasx;
	  else if (RTLD_SUPPORT_LSX)
	    gotplt[0] = (ElfW(Addr)) &_dl_runtime_profile_lsx;
	  else
# endif
	    gotplt[0] = (ElfW(Addr)) &_dl_runtime_profile;

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
#if !defined __loongarch_soft_float
	  if (RTLD_SUPPORT_LASX)
	    gotplt[0] = (ElfW(Addr)) &_dl_runtime_resolve_lasx;
	  else if (RTLD_SUPPORT_LSX)
	    gotplt[0] = (ElfW(Addr)) &_dl_runtime_resolve_lsx;
	  else
#endif
	    gotplt[0] = (ElfW(Addr)) &_dl_runtime_resolve;
	}
      gotplt[1] = (ElfW (Addr)) l;
    }
#endif

  return lazy;
}

#endif /* RESOLVE_MAP */
