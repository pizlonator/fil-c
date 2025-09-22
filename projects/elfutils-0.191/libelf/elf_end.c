/* Free resources associated with Elf descriptor.
   Copyright (C) 1998,1999,2000,2001,2002,2004,2005,2007,2015,2016 Red Hat, Inc.
   Copyright (C) 2023 Mark J. Wielaard <mark@klomp.org>
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <assert.h>
#include <search.h>
#include <stddef.h>
#include <stdlib.h>

#include "libelfP.h"


static void
free_chunk (void *n)
{
  Elf_Data_Chunk *rawchunk = (Elf_Data_Chunk *)n;
  if (rawchunk->dummy_scn.flags & ELF_F_MALLOCED)
    free (rawchunk->data.d.d_buf);
  free (rawchunk);
}

int
elf_end (Elf *elf)
{
  Elf *parent;

  if (elf == NULL)
    /* This is allowed and is a no-op.  */
    return 0;

  /* Make sure we are alone.  */
  rwlock_wrlock (elf->lock);

  if (elf->ref_count != 0 && --elf->ref_count != 0)
    {
      /* Not yet the last activation.  */
      int result = elf->ref_count;
      rwlock_unlock (elf->lock);
      return result;
    }

  if (elf->kind == ELF_K_AR)
    {
      /* We cannot remove the descriptor now since we still have some
	 descriptors which depend on it.  But we can free the archive
	 symbol table since this is only available via the archive ELF
	 descriptor.  The long name table cannot be freed yet since
	 the archive headers for the ELF files in the archive point
	 into this array.  */
      if (elf->state.ar.ar_sym != (Elf_Arsym *) -1l)
	free (elf->state.ar.ar_sym);
      elf->state.ar.ar_sym = NULL;

      if (elf->state.ar.children != NULL)
	{
	  rwlock_unlock(elf->lock);
	  return 0;
	}
    }

  /* Remove this structure from the children list.  */
  parent = elf->parent;
  if (parent != NULL)
    {
      /* This is tricky.  Lock must be acquire from the father to
	 the child but here we already have the child lock.  We
	 solve this problem by giving free the child lock.  The
	 state of REF_COUNT==0 is handled all over the library, so
	 this should be ok.  */
      rwlock_unlock (elf->lock);
      rwlock_rdlock (parent->lock);
      rwlock_wrlock (elf->lock);

      if (parent->state.ar.children == elf)
	parent->state.ar.children = elf->next;
      else
	{
	  struct Elf *child = parent->state.ar.children;

	  while (child->next != elf)
	    child = child->next;

	  child->next = elf->next;
	}

      rwlock_unlock (parent->lock);
    }

  /* This was the last activation.  Free all resources.  */
  switch (elf->kind)
    {
    case ELF_K_AR:
      if (elf->state.ar.long_names != NULL)
	free (elf->state.ar.long_names);
      break;

    case ELF_K_ELF:
      {
	void *rawchunks
	  = (elf->class == ELFCLASS32
	     || (offsetof (struct Elf, state.elf32.rawchunks)
		 == offsetof (struct Elf, state.elf64.rawchunks))
	     ? elf->state.elf32.rawchunks
	     : elf->state.elf64.rawchunks);
	tdestroy (rawchunks, free_chunk);

	Elf_ScnList *list = (elf->class == ELFCLASS32
			     || (offsetof (struct Elf, state.elf32.scns)
				 == offsetof (struct Elf, state.elf64.scns))
			     ? &elf->state.elf32.scns
			     : &elf->state.elf64.scns);

	do
	  {
	    /* Free all separately allocated section headers.  */
	    size_t cnt = list->max;

	    while (cnt-- > 0)
	      {
		/* These pointers can be NULL; it's safe to use
		   'free' since it will check for this.  */
		Elf_Scn *scn = &list->data[cnt];
		Elf_Data_List *runp;

		if ((scn->shdr_flags & ELF_F_MALLOCED) != 0)
		  /* It doesn't matter which pointer.  */
		  free (scn->shdr.e32);

		/* Free zdata if uncompressed, but not yet used as
		   rawdata_base.  If it is already used it will be
		   freed below.  */
		if (scn->zdata_base != scn->rawdata_base)
		  {
		    free (scn->zdata_base);
		    scn->zdata_base = NULL;
		  }

		/* If the file has the same byte order and the
		   architecture doesn't require overly stringent
		   alignment the raw data buffer is the same as the
		   one used for presenting to the caller.  */
		if (scn->data_base != scn->rawdata_base)
		  free (scn->data_base);

		/* The section data is allocated if we couldn't mmap
		   the file.  Or if we had to decompress.  */
		if (elf->map_address == NULL
		    || scn->rawdata_base == scn->zdata_base
		    || (scn->flags & ELF_F_MALLOCED) != 0)
		  free (scn->rawdata_base);

		/* Free the list of data buffers for the section.
		   We don't free the buffers themselves since this
		   is the users job.  */
		runp = scn->data_list.next;
		while (runp != NULL)
		  {
		    Elf_Data_List *oldp = runp;
		    runp = runp->next;
		    if ((oldp->flags & ELF_F_MALLOCED) != 0)
		      free (oldp);
		  }
	      }

	    /* Free the memory for the array.  */
	    Elf_ScnList *oldp = list;
	    list = list->next;
	    assert (list == NULL || oldp->cnt == oldp->max);
	    if (oldp != (elf->class == ELFCLASS32
			 || (offsetof (struct Elf, state.elf32.scns)
			     == offsetof (struct Elf, state.elf64.scns))
			 ? &elf->state.elf32.scns
			 : &elf->state.elf64.scns))
	      free (oldp);
	  }
	while (list != NULL);
      }

      /* Free the section header.  */
      if (elf->state.elf.shdr_malloced  != 0)
	free (elf->class == ELFCLASS32
	      || (offsetof (struct Elf, state.elf32.shdr)
		  == offsetof (struct Elf, state.elf64.shdr))
	      ? (void *) elf->state.elf32.shdr
	      : (void *) elf->state.elf64.shdr);

      /* Free the program header.  */
      if ((elf->state.elf.phdr_flags & ELF_F_MALLOCED) != 0)
	free (elf->class == ELFCLASS32
	      || (offsetof (struct Elf, state.elf32.phdr)
		  == offsetof (struct Elf, state.elf64.phdr))
	      ? (void *) elf->state.elf32.phdr
	      : (void *) elf->state.elf64.phdr);
      break;

    default:
      break;
    }

  if (elf->map_address != NULL && parent == NULL)
    {
      /* The file was read or mapped for this descriptor.  */
      if ((elf->flags & ELF_F_MALLOCED) != 0)
	free (elf->map_address);
      else if ((elf->flags & ELF_F_MMAPPED) != 0)
	munmap (elf->map_address, elf->maximum_size);
    }

  rwlock_unlock (elf->lock);
  rwlock_fini (elf->lock);

  /* Finally the descriptor itself.  */
  free (elf);

  return (parent != NULL && parent->ref_count == 0
	  ? INTUSE(elf_end) (parent) : 0);
}
INTDEF(elf_end)
