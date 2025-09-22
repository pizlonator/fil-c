/* Read all of the file associated with the descriptor.
   Copyright (C) 1998-2009, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <errno.h>
#include <sys/stat.h>

#include "libelfP.h"
#include "common.h"


static void
set_address (Elf *elf, size_t offset)
{
  if (elf->kind == ELF_K_AR)
    {
      Elf *child = elf->state.ar.children;

      while (child != NULL)
	{
	  if (child->map_address == NULL)
	    {
	      child->map_address = elf->map_address;
	      child->start_offset -= offset;
	      if (child->kind == ELF_K_AR)
		child->state.ar.offset -= offset;

	      set_address (child, offset);
	    }

	  child = child->next;
	}
    }
}


char *
internal_function
__libelf_readall (Elf *elf)
{
  /* Get the file.  */
  rwlock_wrlock (elf->lock);

  if (elf->map_address == NULL && unlikely (elf->fildes == -1))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      rwlock_unlock (elf->lock);
      return NULL;
    }

  /* If the file is not mmap'ed and not previously loaded, do it now.  */
  if (elf->map_address == NULL)
    {
      char *mem = NULL;

      /* If this is an archive and we have derived descriptors get the
	 locks for all of them.  */
      libelf_acquire_all (elf);

      if (elf->maximum_size == ~((size_t) 0))
	{
	  /* We don't yet know how large the file is.   Determine that now.  */
	  struct stat st;

	  if (fstat (elf->fildes, &st) < 0)
	    goto read_error;

	  if (sizeof (size_t) >= sizeof (st.st_size)
	      || st.st_size <= ~((size_t) 0))
	    elf->maximum_size = (size_t) st.st_size;
	  else
	    {
	      errno = EOVERFLOW;
	      goto read_error;
	    }
	}

      /* Allocate all the memory we need.  */
      mem = malloc (elf->maximum_size);
      if (mem != NULL)
	{
	  /* Read the file content.  */
	  if (unlikely ((size_t) pread_retry (elf->fildes, mem,
					      elf->maximum_size,
					      elf->start_offset)
			!= elf->maximum_size))
	    {
	      /* Something went wrong.  */
	    read_error:
	      __libelf_seterrno (ELF_E_READ_ERROR);
	      free (mem);
	    }
	  else
	    {
	      /* Remember the address.  */
	      elf->map_address = mem;

	      /* Also remember that we allocated the memory.  */
	      elf->flags |= ELF_F_MALLOCED;

	      /* Propagate the information down to all children and
		 their children.  */
	      set_address (elf, elf->start_offset);

	      /* Correct the own offsets.  */
	      if (elf->kind == ELF_K_AR)
		elf->state.ar.offset -= elf->start_offset;
	      elf->start_offset = 0;
	    }
	}
      else
	__libelf_seterrno (ELF_E_NOMEM);

      /* Free the locks on the children.  */
      libelf_release_all (elf);
    }

  rwlock_unlock (elf->lock);

  return (char *) elf->map_address;
}
