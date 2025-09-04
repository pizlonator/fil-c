/* On-demand PLT fixup for shared objects.
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

#include <alloca.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <ldsodefs.h>
#include <sysdep-cancel.h>
#include "dynamic-link.h"
#include <tls.h>
#include <dl-irel.h>
#include <dl-runtime.h>


/* This function is called through a special trampoline from the PLT the
   first time each PLT entry is called.  We must perform the relocation
   specified in the PLT of the given shared object, and return the resolved
   function address to the trampoline, which will restart the original call
   to that address.  Future calls will bounce directly from the PLT to the
   function.  */

DL_FIXUP_VALUE_TYPE
attribute_hidden __attribute ((noinline)) DL_ARCH_FIXUP_ATTRIBUTE
_dl_fixup (
# ifdef ELF_MACHINE_RUNTIME_FIXUP_ARGS
	   ELF_MACHINE_RUNTIME_FIXUP_ARGS,
# endif
	   struct link_map *l, ElfW(Word) reloc_arg)
{
  const ElfW(Sym) *const symtab
    = (const void *) D_PTR (l, l_info[DT_SYMTAB]);
  const char *strtab = (const void *) D_PTR (l, l_info[DT_STRTAB]);

  const uintptr_t pltgot = (uintptr_t) D_PTR (l, l_info[DT_PLTGOT]);

  const PLTREL *const reloc
    = (const void *) (D_PTR (l, l_info[DT_JMPREL])
		      + reloc_offset (pltgot, reloc_arg));
  const ElfW(Sym) *sym = &symtab[ELFW(R_SYM) (reloc->r_info)];
  const ElfW(Sym) *refsym = sym;
  void *const rel_addr = (void *)(l->l_addr + reloc->r_offset);
  lookup_t result;
  DL_FIXUP_VALUE_TYPE value;

  /* Sanity check that we're really looking at a PLT relocation.  */
  assert (ELFW(R_TYPE)(reloc->r_info) == ELF_MACHINE_JMP_SLOT);

   /* Look up the target symbol.  If the normal lookup rules are not
      used don't look in the global scope.  */
  if (__builtin_expect (ELFW(ST_VISIBILITY) (sym->st_other), 0) == 0)
    {
      const struct r_found_version *version = NULL;

      if (l->l_info[VERSYMIDX (DT_VERSYM)] != NULL)
	{
	  const ElfW(Half) *vernum =
	    (const void *) D_PTR (l, l_info[VERSYMIDX (DT_VERSYM)]);
	  ElfW(Half) ndx = vernum[ELFW(R_SYM) (reloc->r_info)] & 0x7fff;
	  version = &l->l_versions[ndx];
	  if (version->hash == 0)
	    version = NULL;
	}

      /* We need to keep the scope around so do some locking.  This is
	 not necessary for objects which cannot be unloaded or when
	 we are not using any threads (yet).  */
      int flags = DL_LOOKUP_ADD_DEPENDENCY;
      if (!RTLD_SINGLE_THREAD_P)
	{
	  THREAD_GSCOPE_SET_FLAG ();
	  flags |= DL_LOOKUP_GSCOPE_LOCK;
	}

#ifdef RTLD_ENABLE_FOREIGN_CALL
      RTLD_ENABLE_FOREIGN_CALL;
#endif

      result = _dl_lookup_symbol_x (strtab + sym->st_name, l, &sym, l->l_scope,
				    version, ELF_RTYPE_CLASS_PLT, flags, NULL);

      /* We are done with the global scope.  */
      if (!RTLD_SINGLE_THREAD_P)
	THREAD_GSCOPE_RESET_FLAG ();

#ifdef RTLD_FINALIZE_FOREIGN_CALL
      RTLD_FINALIZE_FOREIGN_CALL;
#endif

      /* Currently result contains the base load address (or link map)
	 of the object that defines sym.  Now add in the symbol
	 offset.  */
      value = DL_FIXUP_MAKE_VALUE (result,
				   SYMBOL_ADDRESS (result, sym, false));
    }
  else
    {
      /* We already found the symbol.  The module (and therefore its load
	 address) is also known.  */
      value = DL_FIXUP_MAKE_VALUE (l, SYMBOL_ADDRESS (l, sym, true));
      result = l;
    }

  /* And now perhaps the relocation addend.  */
  value = elf_machine_plt_value (l, reloc, value);

  if (sym != NULL
      && __builtin_expect (ELFW(ST_TYPE) (sym->st_info) == STT_GNU_IFUNC, 0))
    value = elf_ifunc_invoke (DL_FIXUP_VALUE_ADDR (value));

#ifdef SHARED
  /* Auditing checkpoint: we have a new binding.  Provide the auditing
     libraries the possibility to change the value and tell us whether further
     auditing is wanted.
     The l_reloc_result is only allocated if there is an audit module which
     provides a la_symbind.  */
  if (l->l_reloc_result != NULL)
    {
      /* This is the address in the array where we store the result of previous
	 relocations.  */
      struct reloc_result *reloc_result
	= &l->l_reloc_result[reloc_index (pltgot, reloc_arg, sizeof (PLTREL))];
      unsigned int init = atomic_load_acquire (&reloc_result->init);
      if (init == 0)
	{
	  _dl_audit_symbind (l, reloc_result, reloc, sym, &value, result, true);

	  /* Store the result for later runs.  */
	  if (__glibc_likely (! GLRO(dl_bind_not)))
	    {
	      reloc_result->addr = value;
	      /* Guarantee all previous writes complete before init is
		 updated.  See CONCURRENCY NOTES below.  */
	      atomic_store_release (&reloc_result->init, 1);
	    }
	}
      else
	value = reloc_result->addr;
    }
#endif

  /* Finally, fix up the plt itself.  */
  if (__glibc_unlikely (GLRO(dl_bind_not)))
    return value;

  return elf_machine_fixup_plt (l, result, refsym, sym, reloc, rel_addr, value);
}

#if !defined PROF && defined SHARED
DL_FIXUP_VALUE_TYPE
__attribute ((noinline))
DL_ARCH_FIXUP_ATTRIBUTE
_dl_profile_fixup (
# ifdef ELF_MACHINE_RUNTIME_FIXUP_ARGS
		   ELF_MACHINE_RUNTIME_FIXUP_ARGS,
# endif
		   struct link_map *l, ElfW(Word) reloc_arg,
		   ElfW(Addr) retaddr, void *regs, long int *framesizep)
{
  void (*mcount_fct) (ElfW(Addr), ElfW(Addr)) = _dl_mcount;

  if (l->l_reloc_result == NULL)
    {
      /* BZ #14843: ELF_DYNAMIC_RELOCATE is called before l_reloc_result
	 is allocated.  We will get here if ELF_DYNAMIC_RELOCATE calls a
	 resolver function to resolve an IRELATIVE relocation and that
	 resolver calls a function that is not yet resolved (lazy).  For
	 example, the resolver in x86-64 libm.so calls __get_cpu_features
	 defined in libc.so.  Skip audit and resolve the external function
	 in this case.  */
      *framesizep = -1;
      return _dl_fixup (
# ifdef ELF_MACHINE_RUNTIME_FIXUP_ARGS
#  ifndef ELF_MACHINE_RUNTIME_FIXUP_PARAMS
#   error Please define ELF_MACHINE_RUNTIME_FIXUP_PARAMS.
#  endif
			ELF_MACHINE_RUNTIME_FIXUP_PARAMS,
# endif
			l, reloc_arg);
    }

  const uintptr_t pltgot = (uintptr_t) D_PTR (l, l_info[DT_PLTGOT]);

  /* This is the address in the array where we store the result of previous
     relocations.  */
  struct reloc_result *reloc_result
    = &l->l_reloc_result[reloc_index (pltgot, reloc_arg, sizeof (PLTREL))];

 /* CONCURRENCY NOTES:

  Multiple threads may be calling the same PLT sequence and with
  LD_AUDIT enabled they will be calling into _dl_profile_fixup to
  update the reloc_result with the result of the lazy resolution.
  The reloc_result guard variable is reloc_init, and we use
  acquire/release loads and store to it to ensure that the results of
  the structure are consistent with the loaded value of the guard.
  This does not fix all of the data races that occur when two or more
  threads read reloc_result->reloc_init with a value of zero and read
  and write to that reloc_result concurrently.  The expectation is
  generally that while this is a data race it works because the
  threads write the same values.  Until the data races are fixed
  there is a potential for problems to arise from these data races.
  The reloc result updates should happen in parallel but there should
  be an atomic RMW which does the final update to the real result
  entry (see bug 23790).

  The following code uses reloc_result->init set to 0 to indicate if it is
  the first time this object is being relocated, otherwise 1 which
  indicates the object has already been relocated.

  Reading/Writing from/to reloc_result->reloc_init must not happen
  before previous writes to reloc_result complete as they could
  end-up with an incomplete struct.  */
  DL_FIXUP_VALUE_TYPE value;
  unsigned int init = atomic_load_acquire (&reloc_result->init);

  if (init == 0)
    {
      /* This is the first time we have to relocate this object.  */
      const ElfW(Sym) *const symtab
	= (const void *) D_PTR (l, l_info[DT_SYMTAB]);
      const char *strtab = (const char *) D_PTR (l, l_info[DT_STRTAB]);

      const uintptr_t pltgot = (uintptr_t) D_PTR (l, l_info[DT_PLTGOT]);

      const PLTREL *const reloc
	= (const void *) (D_PTR (l, l_info[DT_JMPREL])
			  + reloc_offset (pltgot, reloc_arg));
      const ElfW(Sym) *refsym = &symtab[ELFW(R_SYM) (reloc->r_info)];
      const ElfW(Sym) *defsym = refsym;
      lookup_t result;

      /* Sanity check that we're really looking at a PLT relocation.  */
      assert (ELFW(R_TYPE)(reloc->r_info) == ELF_MACHINE_JMP_SLOT);

      /* Look up the target symbol.  If the symbol is marked STV_PROTECTED
	 don't look in the global scope.  */
      if (__builtin_expect (ELFW(ST_VISIBILITY) (refsym->st_other), 0) == 0)
	{
	  const struct r_found_version *version = NULL;

	  if (l->l_info[VERSYMIDX (DT_VERSYM)] != NULL)
	    {
	      const ElfW(Half) *vernum =
		(const void *) D_PTR (l, l_info[VERSYMIDX (DT_VERSYM)]);
	      ElfW(Half) ndx = vernum[ELFW(R_SYM) (reloc->r_info)] & 0x7fff;
	      version = &l->l_versions[ndx];
	      if (version->hash == 0)
		version = NULL;
	    }

	  /* We need to keep the scope around so do some locking.  This is
	     not necessary for objects which cannot be unloaded or when
	     we are not using any threads (yet).  */
	  int flags = DL_LOOKUP_ADD_DEPENDENCY;
	  if (!RTLD_SINGLE_THREAD_P)
	    {
	      THREAD_GSCOPE_SET_FLAG ();
	      flags |= DL_LOOKUP_GSCOPE_LOCK;
	    }

	  result = _dl_lookup_symbol_x (strtab + refsym->st_name, l,
					&defsym, l->l_scope, version,
					ELF_RTYPE_CLASS_PLT, flags, NULL);

	  /* We are done with the global scope.  */
	  if (!RTLD_SINGLE_THREAD_P)
	    THREAD_GSCOPE_RESET_FLAG ();

	  /* Currently result contains the base load address (or link map)
	     of the object that defines sym.  Now add in the symbol
	     offset.  */
	  value = DL_FIXUP_MAKE_VALUE (result,
				       SYMBOL_ADDRESS (result, defsym, false));

	  if (defsym != NULL
	      && __builtin_expect (ELFW(ST_TYPE) (defsym->st_info)
				   == STT_GNU_IFUNC, 0))
	    value = elf_ifunc_invoke (DL_FIXUP_VALUE_ADDR (value));
	}
      else
	{
	  /* We already found the symbol.  The module (and therefore its load
	     address) is also known.  */
	  value = DL_FIXUP_MAKE_VALUE (l, SYMBOL_ADDRESS (l, refsym, true));

	  if (__builtin_expect (ELFW(ST_TYPE) (refsym->st_info)
				== STT_GNU_IFUNC, 0))
	    value = elf_ifunc_invoke (DL_FIXUP_VALUE_ADDR (value));

	  result = l;
	}
      /* And now perhaps the relocation addend.  */
      value = elf_machine_plt_value (l, reloc, value);

      /* Auditing checkpoint: we have a new binding.  Provide the
	 auditing libraries the possibility to change the value and
	 tell us whether further auditing is wanted.  */
      if (defsym != NULL && GLRO(dl_naudit) > 0)
	_dl_audit_symbind (l, reloc_result, reloc, defsym, &value, result,
			   true);

      /* Store the result for later runs.  */
      if (__glibc_likely (! GLRO(dl_bind_not)))
	{
	  reloc_result->addr = value;
	  /* Guarantee all previous writes complete before
	     init is updated.  See CONCURRENCY NOTES earlier  */
	  atomic_store_release (&reloc_result->init, 1);
	}
      init = 1;
    }
  else
    value = reloc_result->addr;

  /* By default we do not call the pltexit function.  */
  long int framesize = -1;


  /* Auditing checkpoint: report the PLT entering and allow the
     auditors to change the value.  */
  _dl_audit_pltenter (l, reloc_result, &value, regs, &framesize);

  /* Store the frame size information.  */
  *framesizep = framesize;

  (*mcount_fct) (retaddr, DL_FIXUP_VALUE_CODE_ADDR (value));

  return value;
}

#endif /* !defined PROF && defined SHARED */
