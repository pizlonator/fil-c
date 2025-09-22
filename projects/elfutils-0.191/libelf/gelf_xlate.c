/* Transformation functions for ELF data types.
   Copyright (C) 1998,1999,2000,2002,2004,2005,2006,2007,2015 Red Hat, Inc.
   Copyright (C) 2022 Mark J. Wielaard <mark@klomp.org>
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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS	32
#endif


/* Well, what shall I say.  Nothing to do here.  */
#define elf_cvt_Byte(dest, src, n) \
  (__builtin_constant_p (n) && (n) == 1					      \
   ? (void) (*((char *) (dest)) = *((char *) (src)))			      \
   : Elf32_cvt_Byte (dest, src, n))
static void
(elf_cvt_Byte) (void *dest, const void *src, size_t n,
		int encode __attribute__ ((unused)))
{
  if (n != 0)
    memmove (dest, src, n);
}


/* We'll optimize the definition of the conversion functions here a
   bit.  We need only functions for 16, 32, and 64 bits.  The
   functions referenced in the table will be aliases for one of these
   functions.  Which one is decided by the ELFxx_FSZ_type.  */

#if ALLOW_UNALIGNED

#define FETCH(Bits, ptr)	(*(const uint##Bits##_t *) ptr)
#define STORE(Bits, ptr, val)	(*(uint##Bits##_t *) ptr = val)

#else

union unaligned
  {
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
  } attribute_packed;

#define FETCH(Bits, ptr)	(((const union unaligned *) ptr)->u##Bits)
#define STORE(Bits, ptr, val)	(((union unaligned *) ptr)->u##Bits = val)

#endif

/* Now define the conversion functions for the basic types.  We use here
   the fact that file and memory types are the same and that we have the
   ELFxx_FSZ_* macros.

   At the same time we define inline functions which we will use to
   convert the complex types.  */
#define FUNDAMENTAL(NAME, Name, Bits) \
  INLINE2 (ELFW2(Bits,FSZ_##NAME), ElfW2(Bits,cvt_##Name), ElfW2(Bits,Name))
#define INLINE2(Bytes, FName, TName) \
  INLINE3 (Bytes, FName, TName)
#define INLINE3(Bytes, FName, TName)					      \
  static inline void FName##1 (void *dest, const void *ptr)		      \
  {									      \
    switch (Bytes)							      \
      {									      \
      case 2: STORE (16, dest, bswap_16 (FETCH (16, ptr))); break;	      \
      case 4: STORE (32, dest, bswap_32 (FETCH (32, ptr))); break;	      \
      case 8: STORE (64, dest, bswap_64 (FETCH (64, ptr))); break;	      \
      default:								      \
	abort ();							      \
      }									      \
  }									      \
									      \
  static void FName (void *dest, const void *ptr, size_t len,		      \
		     int encode __attribute__ ((unused)))		      \
  {									      \
    size_t n = len / sizeof (TName);					      \
    if (dest < ptr)							      \
      while (n-- > 0)							      \
	{								      \
	  FName##1 (dest, ptr);						      \
	  dest += Bytes;						      \
	  ptr += Bytes;							      \
	}								      \
    else								      \
      {									      \
	dest += len;							      \
	ptr += len;							      \
	while (n-- > 0)							      \
	  {								      \
	    ptr -= Bytes;						      \
	    dest -= Bytes;						      \
	    FName##1 (dest, ptr);					      \
	  }								      \
      }									      \
  }


/* Now the tricky part: define the transformation functions for the
   complex types.  We will use the definitions of the types in
   abstract.h.  */
#define START(Bits, Name, EName) \
  static void								      \
  ElfW2 (Bits, cvt_##Name) (void *dest, const void *src, size_t len,	      \
			    int encode __attribute__ ((unused)))	      \
  { ElfW2(Bits, Name) *tdest = (ElfW2(Bits, Name) *) dest;		      \
    ElfW2(Bits, Name) *tsrc = (ElfW2(Bits, Name) *) src;		      \
    size_t sz = sizeof (ElfW2(Bits, Name));				      \
    size_t n;								      \
    for (n = len / sz; n > 0; ++tdest, ++tsrc, --n) {
#define END(Bits, Name)							      \
    }									      \
    if (len % sz > 0) /* Cannot convert partial structures, just copy. */     \
      memmove (dest, src, len % sz);					      \
  }
#define TYPE_EXTRA(Code)
#define TYPE_XLATE(Code) Code
#define TYPE_NAME(Type, Name) TYPE_NAME2 (Type, Name)
#define TYPE_NAME2(Type, Name) Type##1 (&tdest->Name, &tsrc->Name);
#define TYPE(Name, Bits) TYPE2 (Name, Bits)
#define TYPE2(Name, Bits) TYPE3 (Name##Bits)
#define TYPE3(Name) Name (cvt_)

/* Signal that we are generating conversion functions.  */
#define GENERATE_CONVERSION

/* First generate the 32-bit conversion functions.  */
#define LIBELFBITS 32
#include "gelf_xlate.h"

/* Now generate the 64-bit conversion functions.  */
#define LIBELFBITS 64
#include "gelf_xlate.h"


/* We have a few functions which we must create by hand since the sections
   do not contain records of only one type.  */
#include "version_xlate.h"
#include "gnuhash_xlate.h"
#include "note_xlate.h"
#include "chdr_xlate.h"


/* Now the externally visible table with the function pointers.  */
const xfct_t __elf_xfctstom[ELFCLASSNUM - 1][ELF_T_NUM] =
{
      [ELFCLASS32 - 1] = {
#define define_xfcts(Bits) \
	[ELF_T_BYTE]	= elf_cvt_Byte,					      \
	[ELF_T_ADDR]	= ElfW2(Bits, cvt_Addr),			      \
	[ELF_T_DYN]	= ElfW2(Bits, cvt_Dyn),				      \
	[ELF_T_EHDR]	= ElfW2(Bits, cvt_Ehdr),			      \
	[ELF_T_HALF]	= ElfW2(Bits, cvt_Half),			      \
	[ELF_T_OFF]	= ElfW2(Bits, cvt_Off),				      \
	[ELF_T_PHDR]	= ElfW2(Bits, cvt_Phdr),			      \
	[ELF_T_RELA]	= ElfW2(Bits, cvt_Rela),			      \
	[ELF_T_REL]	= ElfW2(Bits, cvt_Rel),				      \
	[ELF_T_SHDR]	= ElfW2(Bits, cvt_Shdr),			      \
	[ELF_T_SWORD]	= ElfW2(Bits, cvt_Sword),			      \
	[ELF_T_SYM]	= ElfW2(Bits, cvt_Sym),				      \
	[ELF_T_WORD]	= ElfW2(Bits, cvt_Word),			      \
	[ELF_T_XWORD]	= ElfW2(Bits, cvt_Xword),			      \
	[ELF_T_SXWORD]	= ElfW2(Bits, cvt_Sxword),			      \
	[ELF_T_VDEF]	= elf_cvt_Verdef,				      \
	[ELF_T_VDAUX]	= elf_cvt_Verdef,				      \
	[ELF_T_VNEED]	= elf_cvt_Verneed,				      \
	[ELF_T_VNAUX]	= elf_cvt_Verneed,				      \
	[ELF_T_NHDR]	= elf_cvt_note4,				      \
	[ELF_T_NHDR8]	= elf_cvt_note8,				      \
	[ELF_T_SYMINFO] = ElfW2(Bits, cvt_Syminfo),			      \
	[ELF_T_MOVE]	= ElfW2(Bits, cvt_Move),			      \
	[ELF_T_LIB]	= ElfW2(Bits, cvt_Lib),				      \
	[ELF_T_AUXV]	= ElfW2(Bits, cvt_auxv_t),			      \
	[ELF_T_CHDR]	= ElfW2(Bits, cvt_chdr),			      \
	[ELF_T_RELR]	= ElfW2(Bits, cvt_Relr)
        define_xfcts (32),
	[ELF_T_GNUHASH] = Elf32_cvt_Word
      },
      [ELFCLASS64 - 1] = {
	define_xfcts (64),
	[ELF_T_GNUHASH] = elf_cvt_gnuhash
      }
};
