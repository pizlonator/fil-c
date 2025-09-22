/* Return the size of an object file type.
   Copyright (C) 1998-2010, 2015 Red Hat, Inc.
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

#include <gelf.h>
#include <stddef.h>

#include "libelfP.h"


/* These are the sizes for all the known types.  */
const size_t __libelf_type_sizes[ELFCLASSNUM - 1][ELF_T_NUM] =
{
    [ELFCLASS32 - 1] = {
#define TYPE_SIZES(LIBELFBITS) \
      [ELF_T_ADDR]	= ELFW2(LIBELFBITS, FSZ_ADDR),			      \
      [ELF_T_OFF]	= ELFW2(LIBELFBITS, FSZ_OFF),			      \
      [ELF_T_BYTE]	= 1,						      \
      [ELF_T_HALF]	= ELFW2(LIBELFBITS, FSZ_HALF),			      \
      [ELF_T_WORD]	= ELFW2(LIBELFBITS, FSZ_WORD),			      \
      [ELF_T_SWORD]	= ELFW2(LIBELFBITS, FSZ_SWORD),			      \
      [ELF_T_XWORD]	= ELFW2(LIBELFBITS, FSZ_XWORD),			      \
      [ELF_T_SXWORD]	= ELFW2(LIBELFBITS, FSZ_SXWORD),		      \
      [ELF_T_EHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Ehdr)),		      \
      [ELF_T_SHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Shdr)),		      \
      [ELF_T_SYM]	= sizeof (ElfW2(LIBELFBITS, Ext_Sym)),		      \
      [ELF_T_REL]	= sizeof (ElfW2(LIBELFBITS, Ext_Rel)),		      \
      [ELF_T_RELA]	= sizeof (ElfW2(LIBELFBITS, Ext_Rela)),		      \
      [ELF_T_PHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Phdr)),		      \
      [ELF_T_DYN]	= sizeof (ElfW2(LIBELFBITS, Ext_Dyn)),		      \
      [ELF_T_VDEF]	= sizeof (ElfW2(LIBELFBITS, Ext_Verdef)),	      \
      [ELF_T_VDAUX]	= sizeof (ElfW2(LIBELFBITS, Ext_Verdaux)),	      \
      [ELF_T_VNEED]	= sizeof (ElfW2(LIBELFBITS, Ext_Verneed)),	      \
      [ELF_T_VNAUX]	= sizeof (ElfW2(LIBELFBITS, Ext_Vernaux)),	      \
      [ELF_T_NHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Nhdr)),		      \
      /* Note the header size is the same, but padding is different.  */      \
      [ELF_T_NHDR8]	= sizeof (ElfW2(LIBELFBITS, Ext_Nhdr)),		      \
      [ELF_T_SYMINFO]	= sizeof (ElfW2(LIBELFBITS, Ext_Syminfo)),	      \
      [ELF_T_MOVE]	= sizeof (ElfW2(LIBELFBITS, Ext_Move)),		      \
      [ELF_T_LIB]	= sizeof (ElfW2(LIBELFBITS, Ext_Lib)),		      \
      [ELF_T_AUXV]	= sizeof (ElfW2(LIBELFBITS, Ext_auxv_t)),	      \
      [ELF_T_CHDR]	= sizeof (ElfW2(LIBELFBITS, Ext_Chdr)),		      \
      [ELF_T_GNUHASH]	= ELFW2(LIBELFBITS, FSZ_WORD),			      \
      [ELF_T_RELR]	= ELFW2(LIBELFBITS, FSZ_RELR)
      TYPE_SIZES (32)
    },
    [ELFCLASS64 - 1] = {
      TYPE_SIZES (64)
    }
};


size_t
gelf_fsize (Elf *elf, Elf_Type type, size_t count, unsigned int version)
{
  /* We do not have differences between file and memory sizes.  Better
     not since otherwise `mmap' would not work.  */
  if (elf == NULL)
    return 0;

  if (version != EV_CURRENT)
    {
      __libelf_seterrno (ELF_E_UNKNOWN_VERSION);
      return 0;
    }

  if (type >= ELF_T_NUM)
    {
      __libelf_seterrno (ELF_E_UNKNOWN_TYPE);
      return 0;
    }

  return count * __libelf_type_sizes[elf->class - 1][type];
}
INTDEF(gelf_fsize)
