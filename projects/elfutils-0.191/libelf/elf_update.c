/* Update data structures for changes and write them out.
   Copyright (C) 1999, 2000, 2001, 2002, 2004, 2005, 2006, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1999.

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

#include <libelf.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "libelfP.h"


static int64_t
write_file (Elf *elf, int64_t size, int change_bo, size_t shnum)
{
  int class = elf->class;

  /* Check the mode bits now, before modification might change them.  */
  struct stat st;
  if (unlikely (fstat (elf->fildes, &st) != 0))
    {
      __libelf_seterrno (ELF_E_WRITE_ERROR);
      return -1;
    }

  /* Adjust the size in any case.  We do this even if we use `write'.
     We cannot do this if this file is in an archive.  We also don't
     do it *now* if we are shortening the file since this would
     prevent programs to use the data of the file in generating the
     new file.  We truncate the file later in this case.  */
  if (elf->parent == NULL
      && (elf->maximum_size == ~((size_t) 0)
	  || (size_t) size > elf->maximum_size)
      && unlikely (ftruncate (elf->fildes, size) != 0))
    {
      __libelf_seterrno (ELF_E_WRITE_ERROR);
      return -1;
    }

  /* Try to map the file if this isn't done yet.  */
  if (elf->map_address == NULL && elf->cmd == ELF_C_WRITE_MMAP)
    {
      elf->map_address = mmap (NULL, size, PROT_READ | PROT_WRITE,
			       MAP_SHARED, elf->fildes, 0);
      if (unlikely (elf->map_address == MAP_FAILED))
	elf->map_address = NULL;
      else
	elf->flags |= ELF_F_MMAPPED;
    }

  if (elf->map_address != NULL)
    {
      /* When using mmap we want to make sure the file content is
	 really there. Only using ftruncate might mean the file is
	 extended, but space isn't allocated yet.  This might cause a
	 SIGBUS once we write into the mmapped space and the disk is
	 full.  In glibc posix_fallocate is required to extend the
	 file and allocate enough space even if the underlying
	 filesystem would normally return EOPNOTSUPP.  But other
	 implementations might not work as expected.  And the glibc
	 fallback case might fail (with unexpected errnos) in some cases.
	 So we only report an error when the call fails and errno is
	 ENOSPC. Otherwise we ignore the error and treat it as just hint.  */
      if (elf->parent == NULL
	  && (elf->maximum_size == ~((size_t) 0)
	      || (size_t) size > elf->maximum_size))
	{
	  if (unlikely (posix_fallocate (elf->fildes, 0, size) != 0))
	    if (errno == ENOSPC)
	      {
		__libelf_seterrno (ELF_E_WRITE_ERROR);
		return -1;
	      }

	  /* Extend the mmap address if needed.  */
	  if (elf->cmd == ELF_C_RDWR_MMAP
	      && (size_t) size > elf->maximum_size)
	    {
#ifdef HAVE_MREMAP
	      if (mremap (elf->map_address, elf->maximum_size,
			  size, 0) == MAP_FAILED)
#endif
		{
		  __libelf_seterrno (ELF_E_WRITE_ERROR);
		  return -1;
		}
	      elf->maximum_size = size;
	    }

	}

      /* The file is mmaped.  */
      if ((class == ELFCLASS32
	   ? __elf32_updatemmap (elf, change_bo, shnum)
	   : __elf64_updatemmap (elf, change_bo, shnum)) != 0)
	/* Some problem while writing.  */
	size = -1;
    }
  else
    {
      /* The file is not mmaped.  */
      if ((class == ELFCLASS32
	   ? __elf32_updatefile (elf, change_bo, shnum)
	   : __elf64_updatefile (elf, change_bo, shnum)) != 0)
	/* Some problem while writing.  */
	size = -1;
    }

  /* Reduce the file size if necessary.  */
  if (size != -1
      && elf->parent == NULL
      && elf->maximum_size != ~((size_t) 0)
      && (size_t) size < elf->maximum_size
      && unlikely (ftruncate (elf->fildes, size) != 0))
    {
      __libelf_seterrno (ELF_E_WRITE_ERROR);
      size = -1;
    }

  /* POSIX says that ftruncate and write may clear the S_ISUID and S_ISGID
     mode bits.  So make sure we restore them afterwards if they were set.
     This is not atomic if someone else chmod's the file while we operate.  */
  if (size != -1
      && unlikely (st.st_mode & (S_ISUID | S_ISGID))
      /* fchmod ignores the bits we cannot change.  */
      && unlikely (fchmod (elf->fildes, st.st_mode) != 0))
    {
      __libelf_seterrno (ELF_E_WRITE_ERROR);
      size = -1;
    }

  if (size != -1 && elf->parent == NULL)
    elf->maximum_size = size;

  return size;
}


int64_t
elf_update (Elf *elf, Elf_Cmd cmd)
{
  size_t shnum;
  int64_t size;
  int change_bo = 0;

  if (cmd != ELF_C_NULL
      && cmd != ELF_C_WRITE
      && unlikely (cmd != ELF_C_WRITE_MMAP))
    {
      __libelf_seterrno (ELF_E_INVALID_CMD);
      return -1;
    }

  if (elf == NULL)
    return -1;

  if (elf->kind != ELF_K_ELF)
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  rwlock_wrlock (elf->lock);

  /* Make sure we have an ELF header.  */
  if (elf->state.elf.ehdr == NULL)
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      size = -1;
      goto out;
    }

  /* Determine the number of sections.  */
  shnum = (elf->state.elf.scns_last->cnt == 0
	   ? 0
	   : 1 + elf->state.elf.scns_last->data[elf->state.elf.scns_last->cnt - 1].index);

  /* Update the ELF descriptor.  First, place the program header.  It
     will come right after the ELF header.  The count the size of all
     sections and finally place the section table.  */
  size = (elf->class == ELFCLASS32
	  ? __elf32_updatenull_wrlock (elf, &change_bo, shnum)
	  : __elf64_updatenull_wrlock (elf, &change_bo, shnum));
  if (likely (size != -1)
      /* See whether we actually have to write out the data.  */
      && (cmd == ELF_C_WRITE || cmd == ELF_C_WRITE_MMAP))
    {
      if (elf->cmd != ELF_C_RDWR
	  && elf->cmd != ELF_C_RDWR_MMAP
	  && elf->cmd != ELF_C_WRITE
	  && unlikely (elf->cmd != ELF_C_WRITE_MMAP))
	{
	  __libelf_seterrno (ELF_E_UPDATE_RO);
	  size = -1;
	}
      else if (unlikely (elf->fildes == -1))
	{
	  /* We closed the file already.  */
	  __libelf_seterrno (ELF_E_FD_DISABLED);
	  size = -1;
	}
      else
	size = write_file (elf, size, change_bo, shnum);
    }

 out:
  rwlock_unlock (elf->lock);

  return size;
}
