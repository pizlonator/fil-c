/* Loader-internal scratch buffer.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* This is the loader-side analogue of <scratch_buffer.h>.  It exists
   because the loader has two constraints that <scratch_buffer.h> does
   not address:

   1. While the active allocator is __minimal_malloc (early startup,
      before __rtld_malloc_init_real has switched in libc's malloc),
      __minimal_free only releases the most-recent allocation -- a
      malloc'd spill would silently leak.

   2. Some loader code paths cannot route a spill through the
      interposable malloc at all because the user malloc may
      recursively re-enter the loader and invalidate state we are
      copying from (the canonical example is _dl_load_cache_lookup
      copying out of the file-backed ld.so.cache mapping).

   The buffer starts in a stack-resident inline area; if the caller
   needs more bytes, the spill is to anonymous mmap (always safe,
   tagged for /proc/self/maps visibility) or to malloc (cheaper, only
   chosen when both the active allocator is real malloc and the
   caller does not pass DL_SCRATCH_NO_MALLOC).

   Typical usage:

     struct dl_scratch_buffer scratch = dl_scratch_buffer_init ();
     dl_scratch_buffer_allocate (&scratch, needed, 0);
     ... use scratch.data ...
     dl_scratch_buffer_free (&scratch);

   The interface is one-shot: every consumer knows the required size
   upfront and calls dl_scratch_buffer_allocate exactly once, so there
   is no incremental-growth model.  A second allocate without an
   intervening free is a programming error and is checked by an
   assertion in _dl_scratch_buffer_allocate.

   On allocation failure dl_scratch_buffer_allocate does not return;
   it raises a loader ENOMEM via _dl_signal_error.  Callers may
   therefore treat scratch.data as valid after a successful return.  */

#ifndef _DL_SCRATCH_BUFFER_H
#define _DL_SCRATCH_BUFFER_H 1

#include <stdbool.h>
#include <stddef.h>
#include <sys/cdefs.h>

/* Size of the inline area.  Tuned to cover typical ld.so.cache
   entries (well under 256 bytes) so that the common case stays
   entirely on-stack with no syscall and no malloc.  */
enum { DL_SCRATCH_BUFFER_INLINE_SIZE = 256 };

enum dl_scratch_backend
{
  DL_SCRATCH_INLINE,
  DL_SCRATCH_MMAP,
  DL_SCRATCH_MALLOC,
};

struct dl_scratch_buffer
{
  void *data;
  size_t size;
  enum dl_scratch_backend backend;
  char inline_data[DL_SCRATCH_BUFFER_INLINE_SIZE]
    __attribute__ ((aligned (__alignof__ (max_align_t))));
};

enum
{
  /* Forbid the malloc backend for spill allocations -- the spill must
     come from anonymous mmap so that interposed user malloc cannot
     recursively re-enter the loader and invalidate state the caller
     is copying from.  See _dl_load_cache_lookup.  */
  DL_SCRATCH_NO_MALLOC = 1 << 0,
};

/* Return a freshly-initialized scratch buffer suitable for use as a
   stack-resident initializer.  */
static __always_inline __attribute_warn_unused_result__
struct dl_scratch_buffer
dl_scratch_buffer_init (void)
{
  return (struct dl_scratch_buffer) {
    .data = NULL,
    .size = sizeof ((struct dl_scratch_buffer *) 0)->inline_data,
    .backend = DL_SCRATCH_INLINE,
  };
}

extern void _dl_scratch_buffer_allocate (struct dl_scratch_buffer *b,
					 size_t size, unsigned int flags)
  __nonnull ((1)) attribute_hidden;
rtld_hidden_proto (_dl_scratch_buffer_allocate)

extern void _dl_scratch_buffer_free (struct dl_scratch_buffer *b)
  __nonnull ((1)) attribute_hidden;
rtld_hidden_proto (_dl_scratch_buffer_free)

/* Ensure B->data points to a buffer of at least SIZE bytes; updates
   B->size and B->backend accordingly.  Intended to be called exactly
   once per buffer lifetime (callers know the required size upfront --
   there is no incremental growth model).  Raises a loader ENOMEM
   error via _dl_signal_error on failure -- does not return NULL.  */
static __always_inline __nonnull ((1)) void
dl_scratch_buffer_allocate (struct dl_scratch_buffer *b, size_t size,
			    unsigned int flags)
{
  /* First call after dl_scratch_buffer_init: point .data at the
     caller's inline area now that its address is in scope.  */
  if (__glibc_unlikely (b->data == NULL))
    b->data = b->inline_data;
  if (__glibc_likely (size <= b->size))
    return;
  _dl_scratch_buffer_allocate (b, size, flags);
}

/* Release any out-of-line allocation held by B and restore the
   inline state.  Safe to call multiple times (and on an already-freed
   or freshly-initialized buffer).  */
static __always_inline __nonnull ((1)) void
dl_scratch_buffer_free (struct dl_scratch_buffer *b)
{
  if (__glibc_likely (b->backend == DL_SCRATCH_INLINE))
    return;
  _dl_scratch_buffer_free (b);
}

#endif /* dl-scratch-buffer.h */
