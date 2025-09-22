/* Maintenance of module list in libdwfl.
   Copyright (C) 2005, 2006, 2007, 2008, 2014, 2015 Red Hat, Inc.
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
#include "cfi.h"
#include <search.h>

static void
free_cu (struct dwfl_cu *cu)
{
  if (cu->lines != NULL)
    free (cu->lines);
  free (cu);
}

static void
nofree (void *arg __attribute__ ((unused)))
{
}

static void
free_file (struct dwfl_file *file)
{
  free (file->name);

  /* Close the fd only on the last reference.  */
  if (file->elf != NULL && elf_end (file->elf) == 0 && file->fd != -1)
    close (file->fd);
}

void
internal_function
__libdwfl_module_free (Dwfl_Module *mod)
{
  if (mod->lazy_cu_root != NULL)
    tdestroy (mod->lazy_cu_root, nofree);

  if (mod->aranges != NULL)
    free (mod->aranges);

  if (mod->cu != NULL)
    {
      for (size_t i = 0; i < mod->ncu; ++i)
	free_cu (mod->cu[i]);
      free (mod->cu);
    }

  /* We might have primed the Dwarf_CFI ebl cache with our own ebl
     in __libdwfl_set_cfi. Make sure we don't free it twice.  */
  if (mod->eh_cfi != NULL)
    {
      if (mod->eh_cfi->ebl != NULL && mod->eh_cfi->ebl == mod->ebl)
	mod->eh_cfi->ebl = NULL;
      dwarf_cfi_end (mod->eh_cfi);
    }

  if (mod->dwarf_cfi != NULL)
    {
      if (mod->dwarf_cfi->ebl != NULL && mod->dwarf_cfi->ebl == mod->ebl)
	mod->dwarf_cfi->ebl = NULL;
      /* We don't need to explicitly destroy the dwarf_cfi.
	 That will be done by dwarf_end.  */
    }

  if (mod->dw != NULL)
    {
      INTUSE(dwarf_end) (mod->dw);
      if (mod->alt != NULL)
	{
	  INTUSE(dwarf_end) (mod->alt);
	  if (mod->alt_elf != NULL)
	    elf_end (mod->alt_elf);
	  if (mod->alt_fd != -1)
	    close (mod->alt_fd);
	}
    }

  if (mod->ebl != NULL)
    ebl_closebackend (mod->ebl);

  if (mod->debug.elf != mod->main.elf)
    free_file (&mod->debug);
  free_file (&mod->main);
  free_file (&mod->aux_sym);

  if (mod->build_id_bits != NULL)
    free (mod->build_id_bits);

  if (mod->reloc_info != NULL)
    free (mod->reloc_info);

  free (mod->name);
  free (mod->elfpath);
  free (mod);
}

void
dwfl_report_begin_add (Dwfl *dwfl __attribute__ ((unused)))
{
  /* The lookup table will be cleared on demand, there is nothing we need
     to do here.  */
}
INTDEF (dwfl_report_begin_add)

void
dwfl_report_begin (Dwfl *dwfl)
{
  /* Clear the segment lookup table.  */
  dwfl->lookup_elts = 0;

  for (Dwfl_Module *m = dwfl->modulelist; m != NULL; m = m->next)
    m->gc = true;

  dwfl->offline_next_address = OFFLINE_REDZONE;
}
INTDEF (dwfl_report_begin)

static inline Dwfl_Module *
use (Dwfl_Module *mod, Dwfl_Module **tailp, Dwfl *dwfl)
{
  mod->next = *tailp;
  *tailp = mod;

  if (unlikely (dwfl->lookup_module != NULL))
    {
      free (dwfl->lookup_module);
      dwfl->lookup_module = NULL;
    }

  return mod;
}

/* Report that a module called NAME spans addresses [START, END).
   Returns the module handle, either existing or newly allocated,
   or returns a null pointer for an allocation error.  */
Dwfl_Module *
dwfl_report_module (Dwfl *dwfl, const char *name,
		    GElf_Addr start, GElf_Addr end)
{
  Dwfl_Module **tailp = &dwfl->modulelist, **prevp = tailp;

  for (Dwfl_Module *m = *prevp; m != NULL; m = *(prevp = &m->next))
    {
      if (m->low_addr == start && m->high_addr == end
	  && !strcmp (m->name, name))
	{
	  /* This module is still here.  Move it to the place in the list
	     after the last module already reported.  */
	  *prevp = m->next;
	  m->gc = false;
	  return use (m, tailp, dwfl);
	}

      if (! m->gc)
	tailp = &m->next;
    }

  Dwfl_Module *mod = calloc (1, sizeof *mod);
  if (mod == NULL)
    goto nomem;

  mod->name = strdup (name);
  if (mod->name == NULL)
    {
      free (mod);
    nomem:
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return NULL;
    }

  mod->low_addr = start;
  mod->high_addr = end;
  mod->dwfl = dwfl;

  return use (mod, tailp, dwfl);
}
INTDEF (dwfl_report_module)


/* Finish reporting the current set of modules to the library.
   If REMOVED is not null, it's called for each module that
   existed before but was not included in the current report.
   Returns a nonzero return value from the callback.
   DWFL cannot be used until this function has returned zero.  */
int
dwfl_report_end (Dwfl *dwfl,
		 int (*removed) (Dwfl_Module *, void *,
				 const char *, Dwarf_Addr,
				 void *arg),
		 void *arg)
{
  Dwfl_Module **tailp = &dwfl->modulelist;
  while (*tailp != NULL)
    {
      Dwfl_Module *m = *tailp;
      if (m->gc && removed != NULL)
	{
	  int result = (*removed) (MODCB_ARGS (m), arg);
	  if (result != 0)
	    return result;
	}
      if (m->gc)
	{
	  *tailp = m->next;
	  __libdwfl_module_free (m);
	}
      else
	tailp = &m->next;
    }

  return 0;
}
INTDEF (dwfl_report_end)
