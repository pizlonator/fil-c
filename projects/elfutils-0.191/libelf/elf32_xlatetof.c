/* Convert from memory to file representation.
   Copyright (C) 1998, 1999, 2000, 2002, 2015 Red Hat, Inc.
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
#include <string.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS	32
#endif


Elf_Data *
elfw2(LIBELFBITS, xlatetof) (Elf_Data *dest, const Elf_Data *src,
			     unsigned int encode)
{
  /* First test whether the input data is really suitable for this
     type.  This means, whether there is an integer number of records.
     Note that for this implementation the memory and file size of the
     data types are identical.  */
  size_t recsize = __libelf_type_sizes[ELFW(ELFCLASS,LIBELFBITS) - 1][src->d_type];

  if (src->d_size % recsize != 0)
    {
      __libelf_seterrno (ELF_E_INVALID_DATA);
      return NULL;
    }

  /* Next see whether the converted data fits in the output buffer.  */
  if (src->d_size > dest->d_size)
    {
      __libelf_seterrno (ELF_E_DEST_SIZE);
      return NULL;
    }

  /* Test the encode parameter.  */
  if (encode != ELFDATA2LSB && encode != ELFDATA2MSB)
    {
      __libelf_seterrno (ELF_E_INVALID_ENCODING);
      return NULL;
    }

  /* Determine the translation function to use.

     At this point we make an assumption which is valid for all
     existing implementations so far: the memory and file sizes are
     the same.  This has very important consequences:
     a) The requirement that the source and destination buffer can
	overlap can easily be fulfilled.
     b) We need only one function to convert from and memory to file
	and vice versa since the function only has to copy and/or
	change the byte order.
  */
  if ((BYTE_ORDER == LITTLE_ENDIAN && encode == ELFDATA2LSB)
      || (BYTE_ORDER == BIG_ENDIAN && encode == ELFDATA2MSB))
    {
      /* We simply have to copy since the byte order is the same.  */
      if (src->d_buf != dest->d_buf)
	memmove (dest->d_buf, src->d_buf, src->d_size);
    }
  else
    {
      xfct_t fctp;
      fctp = __elf_xfctstom[ELFW(ELFCLASS, LIBELFBITS) - 1][src->d_type];

      /* Do the real work.  */
      (*fctp) (dest->d_buf, src->d_buf, src->d_size, 1);
    }

  /* Now set the real destination type and length since the operation was
     successful.  */
  dest->d_type = src->d_type;
  dest->d_size = src->d_size;

  return dest;
}
INTDEF(elfw2(LIBELFBITS, xlatetof))
