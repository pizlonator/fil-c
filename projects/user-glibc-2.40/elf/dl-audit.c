/* Audit common functions.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <assert.h>
#include <link.h>
#include <ldsodefs.h>
#include <dl-machine.h>
#include <dl-runtime.h>
#include <dl-fixup-attribute.h>
#include <sys/param.h>

void
_dl_audit_activity_map (struct link_map *l, int action)
{
  struct audit_ifaces *afct = GLRO(dl_audit);
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      if (afct->activity != NULL)
	afct->activity (&link_map_audit_state (l, cnt)->cookie, action);
      afct = afct->next;
    }
}

void
_dl_audit_activity_nsid (Lmid_t nsid, int action)
{
  /* If head is NULL, the namespace has become empty, and the audit interface
     does not give us a way to signal LA_ACT_CONSISTENT for it because the
     first loaded module is used to identify the namespace.  */
  struct link_map *head = GL(dl_ns)[nsid]._ns_loaded;
  if (__glibc_likely (GLRO(dl_naudit) == 0)
      || head == NULL || head->l_auditing)
    return;

  _dl_audit_activity_map (head, action);
}

const char *
_dl_audit_objsearch (const char *name, struct link_map *l, unsigned int code)
{
  if (l == NULL || l->l_auditing || code == 0)
    return name;

  struct audit_ifaces *afct = GLRO(dl_audit);
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      if (afct->objsearch != NULL)
	{
	  struct auditstate *state = link_map_audit_state (l, cnt);
	  name = afct->objsearch (name, &state->cookie, code);
	  if (name == NULL)
	    return NULL;
	}
      afct = afct->next;
   }

  return name;
}

void
_dl_audit_objopen (struct link_map *l, Lmid_t nsid)
{
  if (__glibc_likely (GLRO(dl_naudit) == 0))
    return;

  struct audit_ifaces *afct = GLRO(dl_audit);
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      if (afct->objopen != NULL)
	{
	  struct auditstate *state = link_map_audit_state (l, cnt);
	  state->bindflags = afct->objopen (l, nsid, &state->cookie);
	  l->l_audit_any_plt |= state->bindflags != 0;
	}

      afct = afct->next;
   }
}

void
_dl_audit_objclose (struct link_map *l)
{
  if (__glibc_likely (GLRO(dl_naudit) == 0)
      || GL(dl_ns)[l->l_ns]._ns_loaded->l_auditing)
    return;

  struct audit_ifaces *afct = GLRO(dl_audit);
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      if (afct->objclose != NULL)
	{
	  struct auditstate *state= link_map_audit_state (l, cnt);
	  /* Return value is ignored.  */
	  afct->objclose (&state->cookie);
	}

      afct = afct->next;
    }
}

void
_dl_audit_preinit (struct link_map *l)
{
  if (__glibc_likely (GLRO(dl_naudit) == 0))
    return;

  struct audit_ifaces *afct = GLRO(dl_audit);
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      if (afct->preinit != NULL)
	afct->preinit (&link_map_audit_state (l, cnt)->cookie);
      afct = afct->next;
    }
}

void
_dl_audit_symbind_alt (struct link_map *l, const ElfW(Sym) *ref, void **value,
		       lookup_t result)
{
  if ((l->l_audit_any_plt | result->l_audit_any_plt) == 0)
    return;

  const char *strtab = (const char *) D_PTR (result, l_info[DT_STRTAB]);
  /* Compute index of the symbol entry in the symbol table of the DSO with
     the definition.  */
  unsigned int ndx = (ref - (ElfW(Sym) *) D_PTR (result, l_info[DT_SYMTAB]));

  unsigned int altvalue = 0;
  /* Synthesize a symbol record where the st_value field is the result.  */
  ElfW(Sym) sym = *ref;
  sym.st_value = (ElfW(Addr)) *value;

  struct audit_ifaces *afct = GLRO(dl_audit);
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      struct auditstate *match_audit = link_map_audit_state (l, cnt);
      struct auditstate *result_audit = link_map_audit_state (result, cnt);
      if (afct->symbind != NULL
	  && ((match_audit->bindflags & LA_FLG_BINDFROM) != 0
	      || ((result_audit->bindflags & LA_FLG_BINDTO)
		  != 0)))
	{
	  unsigned int flags = altvalue | LA_SYMB_DLSYM;
	  uintptr_t new_value = afct->symbind (&sym, ndx,
					       &match_audit->cookie,
					       &result_audit->cookie,
					       &flags, strtab + ref->st_name);
	  if (new_value != (uintptr_t) sym.st_value)
	    {
	      altvalue = LA_SYMB_ALTVALUE;
	      sym.st_value = new_value;
	    }

	  afct = afct->next;
	}

      *value = (void *) sym.st_value;
    }
}
rtld_hidden_def (_dl_audit_symbind_alt)

void
_dl_audit_symbind (struct link_map *l, struct reloc_result *reloc_result,
		   const void *reloc, const ElfW(Sym) *defsym,
		   DL_FIXUP_VALUE_TYPE *value, lookup_t result, bool lazy)
{
  bool for_jmp_slot = reloc_result == NULL;

  /* Compute index of the symbol entry in the symbol table of the DSO
     with the definition.  */
  unsigned int boundndx = defsym - (ElfW(Sym) *) D_PTR (result,
							l_info[DT_SYMTAB]);
  if (!for_jmp_slot)
    {
      reloc_result->bound = result;
      reloc_result->boundndx = boundndx;
    }

  if ((l->l_audit_any_plt | result->l_audit_any_plt) == 0)
    {
      /* Set all bits since this symbol binding is not interesting.  */
      if (!for_jmp_slot)
	reloc_result->enterexit = (1u << DL_NNS) - 1;
      return;
    }

  /* Synthesize a symbol record where the st_value field is the result.  */
  ElfW(Sym) sym = *defsym;
  sym.st_value = DL_FIXUP_VALUE_ADDR (*value);

  /* Keep track whether there is any interest in tracing the call in the lower
     two bits.  */
  assert (DL_NNS * 2 <= sizeof (reloc_result->flags) * 8);
  assert ((LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT) == 3);
  uint32_t enterexit = LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT;

  const char *strtab2 = (const void *) D_PTR (result, l_info[DT_STRTAB]);

  unsigned int flags = 0;
  struct audit_ifaces *afct = GLRO(dl_audit);
  uintptr_t new_value = (uintptr_t) sym.st_value;
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      /* XXX Check whether both DSOs must request action or only one */
      struct auditstate *l_state = link_map_audit_state (l, cnt);
      struct auditstate *result_state = link_map_audit_state (result, cnt);
      if ((l_state->bindflags & LA_FLG_BINDFROM) != 0
	  && (result_state->bindflags & LA_FLG_BINDTO) != 0)
	{
	  if (afct->symbind != NULL)
	    {
	      flags |= for_jmp_slot ? LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT
				    : 0;
	      new_value = afct->symbind (&sym, boundndx,
					 &l_state->cookie,
					 &result_state->cookie, &flags,
					 strtab2 + defsym->st_name);
	      if (new_value != (uintptr_t) sym.st_value)
		{
		  flags |= LA_SYMB_ALTVALUE;
		  sym.st_value = for_jmp_slot
		    ? DL_FIXUP_BINDNOW_ADDR_VALUE (new_value) : new_value;
		}
	    }

	  /* Remember the results for every audit library and store a summary
	     in the first two bits.  */
	  enterexit &= flags & (LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT);
	  enterexit |= ((flags & (LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT))
			<< ((cnt + 1) * 2));
	}
      else
	/* If the bind flags say this auditor is not interested, set the bits
	   manually.  */
	enterexit |= ((LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT)
		      << ((cnt + 1) * 2));
      afct = afct->next;
    }

  if (!for_jmp_slot)
    {
      reloc_result->enterexit = enterexit;
      reloc_result->flags = flags;
    }

  if (flags & LA_SYMB_ALTVALUE)
    DL_FIXUP_BINDNOW_RELOC (l, reloc, value, new_value, sym.st_value, lazy);
}

void
_dl_audit_pltenter (struct link_map *l, struct reloc_result *reloc_result,
		    DL_FIXUP_VALUE_TYPE *value, void *regs, long int *framesize)
{
  /* Don't do anything if no auditor wants to intercept this call.  */
  if (GLRO(dl_naudit) == 0
      || (reloc_result->enterexit & LA_SYMB_NOPLTENTER))
    return;

  /* Sanity check:  DL_FIXUP_VALUE_CODE_ADDR (value) should have been
     initialized earlier in this function or in another thread.  */
  assert (DL_FIXUP_VALUE_CODE_ADDR (*value) != 0);
  ElfW(Sym) *defsym = ((ElfW(Sym) *) D_PTR (reloc_result->bound,
					    l_info[DT_SYMTAB])
		       + reloc_result->boundndx);

  /* Set up the sym parameter.  */
  ElfW(Sym) sym = *defsym;
  sym.st_value = DL_FIXUP_VALUE_ADDR (*value);

  /* Get the symbol name.  */
  const char *strtab = (const void *) D_PTR (reloc_result->bound,
					     l_info[DT_STRTAB]);
  const char *symname = strtab + sym.st_name;

  /* Keep track of overwritten addresses.  */
  unsigned int flags = reloc_result->flags;

  struct audit_ifaces *afct = GLRO(dl_audit);
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      if (afct->ARCH_LA_PLTENTER != NULL
	  && (reloc_result->enterexit
	      & (LA_SYMB_NOPLTENTER << (2 * (cnt + 1)))) == 0)
	{
	  long int new_framesize = -1;
	  struct auditstate *l_state = link_map_audit_state (l, cnt);
	  struct auditstate *bound_state
	    = link_map_audit_state (reloc_result->bound, cnt);
	  uintptr_t new_value
	    = afct->ARCH_LA_PLTENTER (&sym, reloc_result->boundndx,
				      &l_state->cookie, &bound_state->cookie,
				      regs, &flags, symname, &new_framesize);
	  if (new_value != (uintptr_t) sym.st_value)
	    {
	      flags |= LA_SYMB_ALTVALUE;
	      sym.st_value = new_value;
	    }

	  /* Remember the results for every audit library and store a summary
	     in the first two bits.  */
	  reloc_result->enterexit |= ((flags & (LA_SYMB_NOPLTENTER
						| LA_SYMB_NOPLTEXIT))
				      << (2 * (cnt + 1)));

	  if ((reloc_result->enterexit & (LA_SYMB_NOPLTEXIT
					  << (2 * (cnt + 1))))
	      == 0 && new_framesize != -1 && *framesize != -2)
	    {
	      /* If this is the first call providing information, use it.  */
	      if (*framesize == -1)
		*framesize = new_framesize;
	      /* If two pltenter calls provide conflicting information, use
		 the larger value.  */
	      else if (new_framesize != *framesize)
		*framesize = MAX (new_framesize, *framesize);
	    }
	}

      afct = afct->next;
    }

  *value = DL_FIXUP_ADDR_VALUE (sym.st_value);
}

void
DL_ARCH_FIXUP_ATTRIBUTE
_dl_audit_pltexit (struct link_map *l, ElfW(Word) reloc_arg,
		   const void *inregs, void *outregs)
{
  const uintptr_t pltgot = (uintptr_t) D_PTR (l, l_info[DT_PLTGOT]);

  /* This is the address in the array where we store the result of previous
     relocations.  */
  // XXX Maybe the bound information must be stored on the stack since
  // XXX with bind_not a new value could have been stored in the meantime.
  struct reloc_result *reloc_result =
    &l->l_reloc_result[reloc_index (pltgot, reloc_arg, sizeof (PLTREL))];
  ElfW(Sym) *defsym = ((ElfW(Sym) *) D_PTR (reloc_result->bound,
					    l_info[DT_SYMTAB])
		       + reloc_result->boundndx);

  /* Set up the sym parameter.  */
  ElfW(Sym) sym = *defsym;
  sym.st_value = DL_FIXUP_VALUE_ADDR (reloc_result->addr);

  /* Get the symbol name.  */
  const char *strtab = (const void *) D_PTR (reloc_result->bound,
					     l_info[DT_STRTAB]);
  const char *symname = strtab + sym.st_name;

  struct audit_ifaces *afct = GLRO(dl_audit);
  for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
    {
      if (afct->ARCH_LA_PLTEXIT != NULL
	  && (reloc_result->enterexit
	      & (LA_SYMB_NOPLTEXIT >> (2 * cnt))) == 0)
	{
	  struct auditstate *l_state = link_map_audit_state (l, cnt);
	  struct auditstate *bound_state
	    = link_map_audit_state (reloc_result->bound, cnt);
	  afct->ARCH_LA_PLTEXIT (&sym, reloc_result->boundndx,
				 &l_state->cookie, &bound_state->cookie,
				 inregs, outregs, symname);
	}

      afct = afct->next;
    }
}
