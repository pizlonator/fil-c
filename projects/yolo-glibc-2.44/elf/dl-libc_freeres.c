/* Deallocating malloc'ed memory from the dynamic loader.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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
#include <dl-find_object.h>

static bool
free_slotinfo (struct dtv_slotinfo_list **elemp)
{
  size_t cnt;

  if (*elemp == NULL)
    /* Nothing here, all is removed (or there never was anything).  */
    return true;

  if (!free_slotinfo (&(*elemp)->next))
    /* We cannot free the entry.  */
    return false;

  /* That cleared our next pointer for us.  */

  for (cnt = 0; cnt < (*elemp)->len; ++cnt)
    if ((*elemp)->slotinfo[cnt].map != NULL)
      /* Still used.  */
      return false;

  /* We can remove the list element.  */
  free (*elemp);
  *elemp = NULL;

  return true;
}

void
__rtld_libc_freeres (void)
{
  struct link_map *l;
  struct r_search_path_elem *d;

  /* Remove all search directories.  */
  d = GL(dl_all_dirs);
  while (d != GLRO(dl_init_all_dirs))
    {
      struct r_search_path_elem *old = d;
      d = d->next;
      free (old);
    }

  for (Lmid_t ns = 0; ns < GL(dl_nns); ++ns)
    {
      for (l = GL(dl_ns)[ns]._ns_loaded; l != NULL; l = l->l_next)
	{
	  struct libname_list *lnp = l->l_libname->next;

	  l->l_libname->next = NULL;

	  /* Remove all additional names added to the objects.  */
	  while (lnp != NULL)
	    {
	      struct libname_list *old = lnp;
	      lnp = lnp->next;
	      if (! old->dont_free)
		free (old);
	    }

	  /* Free the initfini dependency list.  */
	  if (l->l_free_initfini)
	    free (l->l_initfini);
	  l->l_initfini = NULL;
	}

      if (__builtin_expect (GL(dl_ns)[ns]._ns_global_scope_alloc, 0) != 0
	  && (GL(dl_ns)[ns]._ns_main_searchlist->r_nlist
	      // XXX Check whether we need NS-specific initial_searchlist
	      == GLRO(dl_initial_searchlist).r_nlist))
	{
	  /* All object dynamically loaded by the program are unloaded.  Free
	     the memory allocated for the global scope variable.  */
	  struct link_map **old = GL(dl_ns)[ns]._ns_main_searchlist->r_list;

	  /* Put the old map in.  */
	  GL(dl_ns)[ns]._ns_main_searchlist->r_list
	    // XXX Check whether we need NS-specific initial_searchlist
	    = GLRO(dl_initial_searchlist).r_list;
	  /* Signal that the original map is used.  */
	  GL(dl_ns)[ns]._ns_global_scope_alloc = 0;

	  /* Now free the old map.  */
	  free (old);
	}
    }

  /* Free the memory allocated for the dtv slotinfo array.  We can do
     this only if all modules which used this memory are unloaded.  */
#ifdef SHARED
  if (GL(dl_initial_dtv) == NULL)
    /* There was no initial TLS setup, it was set up later when
       it used the normal malloc.  */
    free_slotinfo (&GL(dl_tls_dtv_slotinfo_list));
  else
#endif
    /* The first element of the list does not have to be deallocated.
       It was allocated in the dynamic linker (i.e., with a different
       malloc), and in the static library it's in .bss space.  */
    free_slotinfo (&GL(dl_tls_dtv_slotinfo_list)->next);

  void *scope_free_list = GL(dl_scope_free_list);
  GL(dl_scope_free_list) = NULL;
  free (scope_free_list);

  _dl_find_object_freeres ();
}
