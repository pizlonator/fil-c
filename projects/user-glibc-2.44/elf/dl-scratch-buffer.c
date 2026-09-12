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

#include <dl-scratch-buffer.h>

#include <assert.h>
#include <errno.h>
#include <ldsodefs.h>
#include <libc-pointer-arith.h>
#include <libintl.h>
#include <setvmaname.h>
#include <stdlib.h>
#include <sys/mman.h>

void
_dl_scratch_buffer_allocate (struct dl_scratch_buffer *b, size_t size,
			     unsigned int flags)
{
  /* Enforce the one-shot contract.  */
  assert (b->backend == DL_SCRATCH_INLINE);

  bool use_malloc = !(flags & DL_SCRATCH_NO_MALLOC);
#ifdef SHARED
  /* While __minimal_malloc is the active allocator, __minimal_free
     only releases the most-recent block; route through mmap instead so
     dl_scratch_buffer_free can symmetrically release the spill.  */
  if (!__rtld_malloc_is_complete ())
    use_malloc = false;
#endif

  if (use_malloc)
    {
      void *p = malloc (size);
      if (__glibc_unlikely (p == NULL))
	_dl_signal_error (ENOMEM, NULL, NULL,
			  N_("cannot allocate loader scratch buffer"));
      b->data = p;
      b->size = size;
      b->backend = DL_SCRATCH_MALLOC;
      return;
    }

  size_t map_size = ALIGN_UP (size, GLRO(dl_pagesize));
  void *p = __mmap (NULL, map_size, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_PRIVATE, -1, 0);
  if (__glibc_unlikely (p == MAP_FAILED))
    _dl_signal_error (ENOMEM, NULL, NULL,
		      N_("cannot allocate loader scratch buffer"));
  __set_vma_name (p, map_size, " glibc: loader scratch");
  b->data = p;
  b->size = map_size;
  b->backend = DL_SCRATCH_MMAP;
}
rtld_hidden_def (_dl_scratch_buffer_allocate)

void
_dl_scratch_buffer_free (struct dl_scratch_buffer *b)
{
  switch (b->backend)
    {
    case DL_SCRATCH_MALLOC:
      free (b->data);
      break;
    case DL_SCRATCH_MMAP:
      __munmap (b->data, b->size);
      break;
    case DL_SCRATCH_INLINE:
      /* Unreachable in normal use; guarded by the inline wrapper.  */
      break;
    }
  b->data = b->inline_data;
  b->size = sizeof b->inline_data;
  b->backend = DL_SCRATCH_INLINE;
}
rtld_hidden_def (_dl_scratch_buffer_free)
