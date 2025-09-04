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

#include <config.h>

#define ALIGNALLOC_INLINE _GL_EXTERN_INLINE
#include "alignalloc.h"

#include <limits.h>
#include <stdckdint.h>
#include <stdint.h>

#if !ALIGNALLOC_VIA_ALIGNED_ALLOC
# if HAVE_POSIX_MEMALIGN

/* posix_memalign requires the alignment to be a power-of-two multiple of
   sizeof (void *), whereas alignalloc requires it to be a power of two.
   To make it OK for the latter to call the former, check that
   sizeof (void *) is a power of two, which is true on all known platforms.
   This check is here rather than in alignalloc.h to save the compiler
   the trouble of checking it each time alignalloc.h is included.  */
static_assert (! (sizeof (void *) & (sizeof (void *) - 1)));

# else /* !HAVE_POSIX_MEMALIGN */

/* Return P aligned down to ALIGNMENT, which should be a power of two.  */

static void *
align_down (void *p, idx_t alignment)
{
  char *c = p;
  return c - ((uintptr_t) p & (alignment - 1));
}

/* If alignalloc returned R and the base of the originally-allocated
   storage is less than R - UCHAR_MAX, return the address of a pointer
   holding the base of the originally-allocated storage.  */

static void **
address_of_pointer_to_malloced (unsigned char *r)
{
  /* The pointer P is located at the highest address A such that A is
     aligned for pointers, and A + sizeof P < R so that there is room
     for a 0 byte at R - 1.  This approach assumes UCHAR_MAX is large
     enough so that there is room for P; although true on all
     plausible platforms, check the assumption to be safe.  */
  static_assert (sizeof (void *) + alignof (void *) - 1 <= UCHAR_MAX);

  return align_down (r - 1 - sizeof (void *), alignof (void *));
}

/* Return an ALIGNMENT-aligned pointer to new storage of size SIZE,
   or a null pointer (setting errno) if memory is exhausted.
   ALIGNMENT must be a power of two.
   If SIZE is zero, on success return a unique pointer each time.
   To free storage later, call alignfree.  */

void *
alignalloc (idx_t alignment, idx_t size)
{
  /* malloc (ALIGNMENT + SIZE); if it succeeds, there must be at least
     one byte available before the returned pointer.  It's OK if
     ALIGNMENT + SIZE fits in size_t but not idx_t.  */

  size_t malloc_size;
  unsigned char *q;
  if (ckd_add (&malloc_size, size, alignment)
      || ! (q = malloc (malloc_size)))
    {
      errno = ENOMEM;
      return NULL;
    }

  unsigned char *r = align_down (q + alignment, alignment);
  idx_t offset = r - q;

  if (offset <= UCHAR_MAX)
    r[-1] = offset;
  else
    {
      r[-1] = 0;
      *address_of_pointer_to_malloced (r) = q;
    }

  return r;
}

/* Free storage allocated via alignalloc.  Do nothing if PTR is null.  */

void
alignfree (void *ptr)
{
  if (ptr)
    {
      unsigned char *r = ptr;
      unsigned char offset = r[-1];
      void *q = offset ? r - offset : *address_of_pointer_to_malloced (r);
      free (q);
    }
}

# endif /* ! HAVE_POSIX_MEMALIGN */
#endif /* ! ALIGNALLOC_VIA_ALIGNED_ALLOC */
