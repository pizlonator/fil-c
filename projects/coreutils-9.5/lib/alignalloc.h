/* aligned memory allocation

   Copyright 2022-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert.  */

#ifndef ALIGNALLOC_H_
#define ALIGNALLOC_H_

/* This file uses _GL_INLINE_HEADER_BEGIN, _GL_INLINE, _GL_ATTRIBUTE_ALLOC_SIZE,
   _GL_ATTRIBUTE_MALLOC, _GL_ATTRIBUTE_RETURNS_NONNULL, HAVE_POSIX_MEMALIGN.  */
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <errno.h>
#include <stdlib.h>
#include "idx.h"
#if defined __CHERI_PURE_CAPABILITY__
# include <cheri.h>
#endif

_GL_INLINE_HEADER_BEGIN
#ifndef ALIGNALLOC_INLINE
# define ALIGNALLOC_INLINE _GL_INLINE
#endif

/* Whether aligned_alloc supports any power-of-two alignment,
   returns a nonnull pointer for size-zero allocations,
   and sets errno on failure.  */
#if 2 < __GLIBC__ + (16 <= __GLIBC_MINOR__)
# define ALIGNALLOC_VIA_ALIGNED_ALLOC 1
#else
# define ALIGNALLOC_VIA_ALIGNED_ALLOC 0
#endif

/* Work around AddressSanitizer bug.
   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=104262
   https://lists.llvm.org/pipermail/llvm-commits/Week-of-Mon-20220124/1001910.html
   */
#ifdef __SANITIZE_ADDRESS__
# undef ALIGNALLOC_VIA_ALIGNED_ALLOC
# define ALIGNALLOC_VIA_ALIGNED_ALLOC 0
#endif
#ifdef __has_feature
# if __has_feature (address_sanitizer)
#  undef ALIGNALLOC_VIA_ALIGNED_ALLOC
#  define ALIGNALLOC_VIA_ALIGNED_ALLOC 0
# endif
#endif

#if ALIGNALLOC_VIA_ALIGNED_ALLOC || HAVE_POSIX_MEMALIGN

/* Free storage allocated via alignalloc.  Do nothing if PTR is null.  */

ALIGNALLOC_INLINE void
alignfree (void *ptr)
{
  free (ptr);
}

/* Return an ALIGNMENT-aligned pointer to new storage of size SIZE,
   or a null pointer (setting errno) if memory is exhausted.
   ALIGNMENT must be a power of two.
   If SIZE is zero, on success return a unique pointer each time.
   To free storage later, call alignfree.  */

ALIGNALLOC_INLINE
_GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_ALLOC_SIZE ((2))
/* _GL_ATTRIBUTE_DEALLOC (alignfree, 1) */
void *
alignalloc (idx_t alignment, idx_t size)
{
  if ((size_t) -1 < alignment)
    alignment = (size_t) -1;
  if ((size_t) -1 < size)
    size = (size_t) -1;

# if ALIGNALLOC_VIA_ALIGNED_ALLOC
  return aligned_alloc (alignment, size);
# else
  void *ptr = NULL;
  if (alignment < sizeof (void *))
    alignment = sizeof (void *);
  errno = posix_memalign (&ptr, alignment, size | !size);
#  if defined __CHERI_PURE_CAPABILITY__
  if (ptr != NULL)
    ptr = cheri_bounds_set (ptr, size);
#  endif
  return ptr;
# endif
}

#else /* ! (ALIGNALLOC_VIA_ALIGNED_ALLOC || HAVE_POSIX_MEMALIGN) */

void alignfree (void *);
void *alignalloc (idx_t, idx_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_ALLOC_SIZE ((2))
  _GL_ATTRIBUTE_DEALLOC (alignfree, 1);

#endif

/* Like alignalloc, but die instead of returning a null pointer.  */
void *xalignalloc (idx_t, idx_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_ALLOC_SIZE ((2))
  _GL_ATTRIBUTE_RETURNS_NONNULL /* _GL_ATTRIBUTE_DEALLOC (alignfree, 1) */;

_GL_INLINE_HEADER_END

#endif /* !ALIGNALLOC_H_ */
