/* Get ELF program header table.
   Copyright (C) 1998-2010, 2014, 2015 Red Hat, Inc.
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

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "libelfP.h"
#include "common.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif

ElfW2(LIBELFBITS,Phdr) *
__elfw2(LIBELFBITS,getphdr_wrlock) (Elf *elf)
{
  ElfW2(LIBELFBITS,Phdr) *result;

  /* If the program header entry has already been filled in the code
     below must already have been run.  So the class is set, too.  No
     need to waste any more time here.  */
  result = elf->state.ELFW(elf,LIBELFBITS).phdr;
  if (likely (result != NULL))
    return result;

  if (elf->class == 0)
    elf->class = ELFW(ELFCLASS,LIBELFBITS);
  else if (elf->class != ELFW(ELFCLASS,LIBELFBITS))
    {
      __libelf_seterrno (ELF_E_INVALID_CLASS);
      result = NULL;
      goto out;
    }

  if (likely (result == NULL))
    {
      /* Read the section header table.  */
      ElfW2(LIBELFBITS,Ehdr) *ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

      /* If no program header exists return NULL.  */
      size_t phnum;
      if (__elf_getphdrnum_rdlock (elf, &phnum) != 0)
	goto out;
      if (phnum == 0 || ehdr->e_phoff == 0)
	{
	  __libelf_seterrno (ELF_E_NO_PHDR);
	  goto out;
	}

      /* Check this doesn't overflow.  */
      size_t size = phnum * sizeof (ElfW2(LIBELFBITS,Phdr));

      if (phnum > SIZE_MAX / sizeof (ElfW2(LIBELFBITS,Phdr))
	  || ehdr->e_phoff > elf->maximum_size
	  || elf->maximum_size - ehdr->e_phoff < size)
	{
	  __libelf_seterrno (ELF_E_INVALID_DATA);
	  goto out;
	}

      if (elf->map_address != NULL)
	{
	  /* First see whether the information in the ELF header is
	     valid and it does not ask for too much.  */
	  if (unlikely (ehdr->e_phoff >= elf->maximum_size)
	      || unlikely (elf->maximum_size - ehdr->e_phoff < size))
	    {
	      /* Something is wrong.  */
	      __libelf_seterrno (ELF_E_INVALID_PHDR);
	      goto out;
	    }

	  /* All the data is already mapped.  Use it.  */
	  void *file_phdr = ((char *) elf->map_address
			     + elf->start_offset + ehdr->e_phoff);
	  if (ehdr->e_ident[EI_DATA] == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || ((uintptr_t) file_phdr
		      & (__alignof__ (ElfW2(LIBELFBITS,Phdr)) - 1)) == 0))
	    /* Simply use the mapped data.  */
	    elf->state.ELFW(elf,LIBELFBITS).phdr = file_phdr;
	  else
	    {
	      ElfW2(LIBELFBITS,Phdr) *notcvt;
	      ElfW2(LIBELFBITS,Phdr) *phdr;

	      /* Allocate memory for the program headers.  We know the number
		 of entries from the ELF header.  */
	      phdr = elf->state.ELFW(elf,LIBELFBITS).phdr =
		(ElfW2(LIBELFBITS,Phdr) *) malloc (size);
	      if (elf->state.ELFW(elf,LIBELFBITS).phdr == NULL)
		{
		  __libelf_seterrno (ELF_E_NOMEM);
		  goto out;
		}
	      elf->state.ELFW(elf,LIBELFBITS).phdr_flags |=
		ELF_F_MALLOCED | ELF_F_DIRTY;

	      /* Now copy the data and at the same time convert the
		 byte order.  */

	      if (ehdr->e_ident[EI_DATA] == MY_ELFDATA)
		{
		  assert (! ALLOW_UNALIGNED);
		  memcpy (phdr, file_phdr, size);
		}
	      else
		{
		  bool copy = ! (ALLOW_UNALIGNED
				 || ((uintptr_t) file_phdr
				     & (__alignof__ (ElfW2(LIBELFBITS,Phdr))
					- 1)) == 0);
		  if (! copy)
		    notcvt = file_phdr;
		  else
		    {
		      notcvt = (ElfW2(LIBELFBITS,Phdr) *) malloc (size);
		      if (unlikely (notcvt == NULL))
			{
			  __libelf_seterrno (ELF_E_NOMEM);
			  goto out;
			}
		      memcpy (notcvt, file_phdr, size);
		    }

		  for (size_t cnt = 0; cnt < phnum; ++cnt)
		    {
		      CONVERT_TO (phdr[cnt].p_type, notcvt[cnt].p_type);
		      CONVERT_TO (phdr[cnt].p_offset, notcvt[cnt].p_offset);
		      CONVERT_TO (phdr[cnt].p_vaddr, notcvt[cnt].p_vaddr);
		      CONVERT_TO (phdr[cnt].p_paddr, notcvt[cnt].p_paddr);
		      CONVERT_TO (phdr[cnt].p_filesz, notcvt[cnt].p_filesz);
		      CONVERT_TO (phdr[cnt].p_memsz, notcvt[cnt].p_memsz);
		      CONVERT_TO (phdr[cnt].p_flags, notcvt[cnt].p_flags);
		      CONVERT_TO (phdr[cnt].p_align, notcvt[cnt].p_align);
		    }

		  if (copy)
		    free (notcvt);
		}
	    }
	}
      else if (likely (elf->fildes != -1))
	{
	  /* Allocate memory for the program headers.  We know the number
	     of entries from the ELF header.  */
	  elf->state.ELFW(elf,LIBELFBITS).phdr =
	    (ElfW2(LIBELFBITS,Phdr) *) malloc (size);
	  if (elf->state.ELFW(elf,LIBELFBITS).phdr == NULL)
	    {
	      __libelf_seterrno (ELF_E_NOMEM);
	      goto out;
	    }
	  elf->state.ELFW(elf,LIBELFBITS).phdr_flags |= ELF_F_MALLOCED;

	  /* Read the header.  */
	  ssize_t n = pread_retry (elf->fildes,
				   elf->state.ELFW(elf,LIBELFBITS).phdr, size,
				   elf->start_offset + ehdr->e_phoff);
	  if (unlikely ((size_t) n != size))
	    {
	      /* Severe problems.  We cannot read the data.  */
	      __libelf_seterrno (ELF_E_READ_ERROR);
	      free (elf->state.ELFW(elf,LIBELFBITS).phdr);
	      elf->state.ELFW(elf,LIBELFBITS).phdr = NULL;
	      goto out;
	    }

	  /* If the byte order of the file is not the same as the one
	     of the host convert the data now.  */
	  if (ehdr->e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      ElfW2(LIBELFBITS,Phdr) *phdr
		= elf->state.ELFW(elf,LIBELFBITS).phdr;

	      for (size_t cnt = 0; cnt < phnum; ++cnt)
		{
		  CONVERT (phdr[cnt].p_type);
		  CONVERT (phdr[cnt].p_offset);
		  CONVERT (phdr[cnt].p_vaddr);
		  CONVERT (phdr[cnt].p_paddr);
		  CONVERT (phdr[cnt].p_filesz);
		  CONVERT (phdr[cnt].p_memsz);
		  CONVERT (phdr[cnt].p_flags);
		  CONVERT (phdr[cnt].p_align);
		}
	    }
	}
      else
	{
	  /* The file descriptor was already enabled and not all data was
	     read.  */
	  __libelf_seterrno (ELF_E_FD_DISABLED);
	  goto out;
	}

      result = elf->state.ELFW(elf,LIBELFBITS).phdr;
    }

 out:
  return result;
}

ElfW2(LIBELFBITS,Phdr) *
elfw2(LIBELFBITS,getphdr) (Elf *elf)
{
  ElfW2(LIBELFBITS,Phdr) *result;

  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  /* If the program header entry has already been filled in the code
   * in getphdr_wrlock must already have been run.  So the class is
   * set, too.  No need to waste any more time here.  */
  result = elf->state.ELFW(elf,LIBELFBITS).phdr;
  if (likely (result != NULL))
    return result;

  rwlock_wrlock (elf->lock);
  result = __elfw2(LIBELFBITS,getphdr_wrlock) (elf);
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elfw2(LIBELFBITS,getphdr))
