/* Machine-dependent ELF dynamic relocation inline functions.  RISC-V version.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.
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

#define ELF_MACHINE_NAME "RISC-V"

#include <entry.h>
#include <elf/elf.h>
#include <sys/asm.h>
#include <dl-tls.h>
#include <dl-irel.h>
#include <dl-static-tls.h>
#include <dl-machine-rel.h>

#ifndef _RTLD_PROLOGUE
# define _RTLD_PROLOGUE(entry)						\
	".globl\t" __STRING (entry) "\n\t"				\
	".type\t" __STRING (entry) ", @function\n"			\
	__STRING (entry) ":\n\t"
#endif

#ifndef _RTLD_EPILOGUE
# define _RTLD_EPILOGUE(entry)						\
	".size\t" __STRING (entry) ", . - " __STRING (entry) "\n\t"
#endif

#define ELF_MACHINE_JMP_SLOT R_RISCV_JUMP_SLOT

#define elf_machine_type_class(type)				\
  ((ELF_RTYPE_CLASS_PLT * ((type) == ELF_MACHINE_JMP_SLOT	\
     || (__WORDSIZE == 32 && (type) == R_RISCV_TLS_DTPREL32)	\
     || (__WORDSIZE == 32 && (type) == R_RISCV_TLS_DTPMOD32)	\
     || (__WORDSIZE == 32 && (type) == R_RISCV_TLS_TPREL32)	\
     || (__WORDSIZE == 64 && (type) == R_RISCV_TLS_DTPREL64)	\
     || (__WORDSIZE == 64 && (type) == R_RISCV_TLS_DTPMOD64)	\
     || (__WORDSIZE == 64 && (type) == R_RISCV_TLS_TPREL64)))	\
   | (ELF_RTYPE_CLASS_COPY * ((type) == R_RISCV_COPY)))

/* Return nonzero iff ELF header is compatible with the running host.  */
static inline int __attribute_used__
elf_machine_matches_host (const ElfW(Ehdr) *ehdr)
{
  /* We can only run RISC-V binaries.  */
  if (ehdr->e_machine != EM_RISCV)
    return 0;

  /* Ensure the library's floating-point ABI matches that of the running
     system.  For now we don't support mixing XLEN, so there's no need (or way)
     to check it matches.  */
#ifdef __riscv_float_abi_double
  if ((ehdr->e_flags & EF_RISCV_FLOAT_ABI) != EF_RISCV_FLOAT_ABI_DOUBLE)
    return 0;
#else
  if ((ehdr->e_flags & EF_RISCV_FLOAT_ABI) != EF_RISCV_FLOAT_ABI_SOFT)
    return 0;
#endif

  return 1;
}

/* Return the run-time load address of the shared object.  */
static inline ElfW(Addr)
elf_machine_load_address (void)
{
  extern const ElfW(Ehdr) __ehdr_start attribute_hidden;
  return (ElfW(Addr)) &__ehdr_start;
}

/* Return the link-time address of _DYNAMIC.  */
static inline ElfW(Addr)
elf_machine_dynamic (void)
{
  extern ElfW(Dyn) _DYNAMIC[] attribute_hidden;
  return (ElfW(Addr)) _DYNAMIC - elf_machine_load_address ();
}

#define STRINGXP(X) __STRING (X)
#define STRINGXV(X) STRINGV_ (X)
#define STRINGV_(...) # __VA_ARGS__

/* Initial entry point code for the dynamic linker.
   The C function `_dl_start' is the real entry point;
   its return value is the user program's entry point.  */

#define RTLD_START asm (\
	".text\n\
	" _RTLD_PROLOGUE (ENTRY_POINT) "\
	mv a0, sp\n\
	jal _dl_start\n\
	" _RTLD_PROLOGUE (_dl_start_user) "\
	# Stash user entry point in s0.\n\
	mv s0, a0\n\
	# Load the adjusted argument count.\n\
	" STRINGXP (REG_L) " a1, 0(sp)\n\
	# Call _dl_init (struct link_map *main_map, int argc, char **argv, char **env) \n\
	" STRINGXP (REG_L) " a0, _rtld_local\n\
	add a2, sp, " STRINGXP (SZREG) "\n\
	sll a3, a1, " STRINGXP (PTRLOG) "\n\
	add a3, a3, a2\n\
	add a3, a3, " STRINGXP (SZREG) "\n\
	# Stash the stack pointer in s1.\n\
	mv s1, sp\n\
	# Align stack to 128 bits for the _dl_init call.\n\
	andi sp, sp,-16\n\
	# Call the function to run the initializers.\n\
	jal _dl_init\n\
	# Restore the stack pointer for _start.\n\
	mv sp, s1\n\
	# Pass our finalizer function to _start.\n\
	lla a0, _dl_fini\n\
	# Jump to the user entry point.\n\
	jr s0\n\
	" _RTLD_EPILOGUE (ENTRY_POINT) \
	  _RTLD_EPILOGUE (_dl_start_user) "\
	.previous" \
);

/* Names of the architecture-specific auditing callback functions.  */
#define ARCH_LA_PLTENTER riscv_gnu_pltenter
#define ARCH_LA_PLTEXIT riscv_gnu_pltexit

/* Bias .got.plt entry by the offset requested by the PLT header.  */
#define elf_machine_plt_value(map, reloc, value) (value)

static inline ElfW(Addr)
elf_machine_fixup_plt (struct link_map *map, lookup_t t,
		       const ElfW(Sym) *refsym, const ElfW(Sym) *sym,
		       const ElfW(Rela) *reloc,
		       ElfW(Addr) *reloc_addr, ElfW(Addr) value)
{
  return *reloc_addr = value;
}

#endif /* !dl_machine_h */

#ifdef RESOLVE_MAP

static inline void
__attribute__ ((always_inline))
elf_machine_rela_relative (ElfW(Addr) l_addr, const ElfW(Rela) *reloc,
			  void *const reloc_addr)
{
  /* R_RISCV_RELATIVE might located in debug info section which might not
     aligned to XLEN bytes.  Also support relocations on unaligned offsets.  */
  ElfW(Addr) value = l_addr + reloc->r_addend;
  memcpy (reloc_addr, &value, sizeof value);
}

/* Perform a relocation described by R_INFO at the location pointed to
   by RELOC_ADDR.  SYM is the relocation symbol specified by R_INFO and
   MAP is the object containing the reloc.  */

static inline void
__attribute__ ((always_inline))
elf_machine_rela (struct link_map *map, struct r_scope_elem *scope[],
		  const ElfW(Rela) *reloc, const ElfW(Sym) *sym,
		  const struct r_found_version *version,
		  void *const reloc_addr, int skip_ifunc)
{
  ElfW(Addr) r_info = reloc->r_info;
  const unsigned long int r_type = ELFW (R_TYPE) (r_info);
  ElfW(Addr) *addr_field = (ElfW(Addr) *) reloc_addr;
  const ElfW(Sym) *const __attribute__ ((unused)) refsym = sym;
  struct link_map *sym_map = RESOLVE_MAP (map, scope, &sym, version, r_type);
  ElfW(Addr) value = 0;
  if (sym_map != NULL)
    value = SYMBOL_ADDRESS (sym_map, sym, true) + reloc->r_addend;

  if (sym != NULL
      && __glibc_unlikely (ELFW(ST_TYPE) (sym->st_info) == STT_GNU_IFUNC)
      && __glibc_likely (sym->st_shndx != SHN_UNDEF)
      && __glibc_likely (!skip_ifunc))
    value = elf_ifunc_invoke (value);


  switch (r_type)
    {
    case R_RISCV_RELATIVE:
      elf_machine_rela_relative (map->l_addr, reloc, addr_field);
      break;
    case R_RISCV_JUMP_SLOT:
    case __WORDSIZE == 64 ? R_RISCV_64 : R_RISCV_32:
      *addr_field = value;
      break;

# ifndef RTLD_BOOTSTRAP
    case __WORDSIZE == 64 ? R_RISCV_TLS_DTPMOD64 : R_RISCV_TLS_DTPMOD32:
      if (sym_map)
	*addr_field = sym_map->l_tls_modid;
      break;

    case __WORDSIZE == 64 ? R_RISCV_TLS_DTPREL64 : R_RISCV_TLS_DTPREL32:
      if (sym != NULL)
	*addr_field = TLS_DTPREL_VALUE (sym) + reloc->r_addend;
      break;

    case __WORDSIZE == 64 ? R_RISCV_TLS_TPREL64 : R_RISCV_TLS_TPREL32:
      if (sym != NULL)
	{
	  CHECK_STATIC_TLS (map, sym_map);
	  *addr_field = TLS_TPREL_VALUE (sym_map, sym) + reloc->r_addend;
	}
      break;

    case R_RISCV_COPY:
      {
	if (__glibc_unlikely (sym == NULL))
	  /* This can happen in trace mode if an object could not be
	     found.  */
	  break;

	/* Handle TLS copy relocations.  */
	if (__glibc_unlikely (ELFW (ST_TYPE) (sym->st_info) == STT_TLS))
	  {
	    /* There's nothing to do if the symbol is in .tbss.  */
	    if (__glibc_likely (sym->st_value >= sym_map->l_tls_initimage_size))
	      break;
	    value += (ElfW(Addr)) sym_map->l_tls_initimage - sym_map->l_addr;
	  }

	size_t size = sym->st_size;
	if (__glibc_unlikely (sym->st_size != refsym->st_size))
	  {
	    const char *strtab = (const void *) D_PTR (map, l_info[DT_STRTAB]);
	    if (sym->st_size > refsym->st_size)
	      size = refsym->st_size;
	    if (sym->st_size > refsym->st_size || GLRO(dl_verbose))
	      _dl_error_printf ("\
  %s: Symbol `%s' has different size in shared object, consider re-linking\n",
				rtld_progname ?: "<program name unknown>",
				strtab + refsym->st_name);
	  }

	memcpy (reloc_addr, (void *)value, size);
	break;
      }

    case R_RISCV_IRELATIVE:
      value = map->l_addr + reloc->r_addend;
      if (__glibc_likely (!skip_ifunc))
        value = elf_ifunc_invoke (value);
      *addr_field = value;
      break;

    case R_RISCV_NONE:
      break;
# endif /* !RTLD_BOOTSTRAP */

    default:
      _dl_reloc_bad_type (map, r_type, 0);
      break;
    }
}

static inline void
__attribute__ ((always_inline))
elf_machine_lazy_rel (struct link_map *map, struct r_scope_elem *scope[],
		      ElfW(Addr) l_addr, const ElfW(Rela) *reloc,
		      int skip_ifunc)
{
  ElfW(Addr) *const reloc_addr = (void *) (l_addr + reloc->r_offset);
  const unsigned int r_type = ELFW (R_TYPE) (reloc->r_info);

  /* Check for unexpected PLT reloc type.  */
  if (__glibc_likely (r_type == R_RISCV_JUMP_SLOT))
    {
      if (__glibc_unlikely (map->l_mach.plt == 0))
	{
	  if (l_addr)
	    *reloc_addr += l_addr;
	}
      else
	*reloc_addr = map->l_mach.plt;
    }
  else if (__glibc_unlikely (r_type == R_RISCV_IRELATIVE))
    {
      ElfW(Addr) value = map->l_addr + reloc->r_addend;
      if (__glibc_likely (!skip_ifunc))
        value = elf_ifunc_invoke (value);
      *reloc_addr = value;
    }
  else
    _dl_reloc_bad_type (map, r_type, 1);
}

/* Set up the loaded object described by L so its stub function
   will jump to the on-demand fixup code __dl_runtime_resolve.  */

static inline int
__attribute__ ((always_inline))
elf_machine_runtime_setup (struct link_map *l, struct r_scope_elem *scope[],
			   int lazy, int profile)
{
#ifndef RTLD_BOOTSTRAP
  /* If using PLTs, fill in the first two entries of .got.plt.  */
  if (l->l_info[DT_JMPREL])
    {
      extern void _dl_runtime_resolve (void) __attribute__ ((visibility ("hidden")));
      extern void _dl_runtime_profile (void) __attribute__ ((visibility ("hidden")));
      ElfW(Addr) *gotplt = (ElfW(Addr) *) D_PTR (l, l_info[DT_PLTGOT]);
      /* If a library is prelinked but we have to relocate anyway,
	 we have to be able to undo the prelinking of .got.plt.
	 The prelinker saved the address of .plt for us here.  */
      if (gotplt[1])
	l->l_mach.plt = gotplt[1] + l->l_addr;
      /* The gotplt[0] entry contains the address of a function which gets
	 called to get the address of a so far unresolved function and
	 jump to it.  The profiling extension of the dynamic linker allows
	 to intercept the calls to collect information.  In this case we
	 don't store the address in the GOT so that all future calls also
	 end in this function.  */
#ifdef SHARED
      if (profile != 0)
	{
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
	  gotplt[0] = (ElfW(Addr)) &_dl_runtime_resolve;
	}
      gotplt[1] = (ElfW(Addr)) l;
    }

  if (l->l_type == lt_executable && l->l_relocated)
    {
      /* The __global_pointer$ may not be defined by the linker if the
	 $gp register does not be used to access the global variable
	 in the executable program. Therefore, the search symbol is
	 set to a weak symbol to avoid we error out if the
	 __global_pointer$ is not found.  */
      ElfW(Sym) gp_sym = { 0 };
      gp_sym.st_info = (unsigned char) ELFW (ST_INFO (STB_WEAK, STT_NOTYPE));

      const ElfW(Sym) *ref = &gp_sym;
      _dl_lookup_symbol_x ("__global_pointer$", l, &ref,
			   l->l_scope, NULL, 0, 0, NULL);
      if (ref)
        asm (
          "mv gp, %0\n"
          :
          : "r" (ref->st_value)
        );
    }
#endif
  return lazy;
}

#endif /* RESOLVE_MAP */
