/* Compute simple checksum from permanent parts of the ELF file.
   Copyright (C) 2002, 2003, 2004, 2005, 2009, 2015 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gelf.h"
#include "libelfP.h"
#include "elf-knowledge.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


#define process_block(crc, data) \
  __libelf_crc32 (crc, data->d_buf, data->d_size)


long int
elfw2(LIBELFBITS,checksum) (Elf *elf)
{
  size_t shstrndx;
  Elf_Scn *scn;
  long int result = 0;
  unsigned char *ident;
  bool same_byte_order;

  if (elf == NULL)
    return -1l;

  /* Find the section header string table.  */
  if  (INTUSE(elf_getshdrstrndx) (elf, &shstrndx) < 0)
    {
      /* This can only happen if the ELF handle is not for real.  */
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1l;
    }

  /* Determine whether the byte order of the file and that of the host
     is the same.  */
  ident = elf->state.ELFW(elf,LIBELFBITS).ehdr->e_ident;
  same_byte_order = ((ident[EI_DATA] == ELFDATA2LSB
		      && BYTE_ORDER == LITTLE_ENDIAN)
		     || (ident[EI_DATA] == ELFDATA2MSB
			 && BYTE_ORDER == BIG_ENDIAN));

  /* If we don't have native byte order, we will likely need to
     convert the data with xlate functions.  We do it upfront instead
     of relocking mid-iteration. */
  if (!likely (same_byte_order))
    rwlock_wrlock (elf->lock);
  else
    rwlock_rdlock (elf->lock);

  /* Iterate over all sections to find those which are not strippable.  */
  scn = NULL;
  while ((scn = INTUSE(elf_nextscn) (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr;
      Elf_Data *data;

      /* Get the section header.  */
      shdr = INTUSE(gelf_getshdr) (scn, &shdr_mem);
      if (shdr == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
	  result = -1l;
	  goto out;
	}

      if (SECTION_STRIP_P (shdr,
			   INTUSE(elf_strptr) (elf, shstrndx, shdr->sh_name),
			   true))
	/* The section can be stripped.  Don't use it.  */
	continue;

      /* Do not look at NOBITS sections.  */
      if (shdr->sh_type == SHT_NOBITS)
	continue;

      /* To compute the checksum we need to get to the data.  For
	 repeatable results we must use the external format.  The data
	 we get with 'elf'getdata' might be changed for endianness
	 reasons.  Therefore we use 'elf_rawdata' if possible.  But
	 this function can fail if the data was constructed by the
	 program.  In this case we have to use 'elf_getdata' and
	 eventually convert the data to the external format.  */
      data = INTUSE(elf_rawdata) (scn, NULL);
      if (data != NULL)
	{
	  /* The raw data is available.  */
	  result = process_block (result, data);

	  /* Maybe the user added more data.  These blocks cannot be
	     read using 'elf_rawdata'.  Simply proceed with looking
	     for more data block with 'elf_getdata'.  */
	}

      /* Iterate through the list of data blocks.  */
      while ((data = INTUSE(elf_getdata) (scn, data)) != NULL)
	/* If the file byte order is the same as the host byte order
	   process the buffer directly.  If the data is just a stream
	   of bytes which the library will not convert we can use it
	   as well.  */
	if (likely (same_byte_order) || data->d_type == ELF_T_BYTE)
	  result = process_block (result, data);
	else
	  {
	    /* Convert the data to file byte order.  */
	    if (INTUSE(elfw2(LIBELFBITS,xlatetof)) (data, data, ident[EI_DATA])
		== NULL)
	      {
		result = -1l;
		goto out;
	      }

	    result = process_block (result, data);

	    /* And convert it back.  */
	    if (INTUSE(elfw2(LIBELFBITS,xlatetom)) (data, data, ident[EI_DATA])
		== NULL)
	      {
		result = -1l;
		goto out;
	      }
	  }
    }

 out:
  rwlock_unlock (elf->lock);
  return result;
}
INTDEF(elfw2(LIBELFBITS,checksum))
