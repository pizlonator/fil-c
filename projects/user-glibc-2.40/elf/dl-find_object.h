/* Locating objects in the process image.  ld.so implementation.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#ifndef _DL_FIND_EH_FRAME_H
#define _DL_FIND_EH_FRAME_H

#include <assert.h>
#include <atomic.h>
#include <dlfcn.h>
#include <ldsodefs.h>
#include <stdbool.h>
#include <stdint.h>

/* Internal version of struct dl_find_object.  Does not include the
   (yet unused) flags member.  We need to make a copy of data also in
   struct link_map to support non-contiguous mappings, and to support
   software transactional memory (the link map is not covered by
   transactions).  */
struct dl_find_object_internal
{
  uintptr_t map_start;
  uintptr_t map_end;            /* Set to map_start by dlclose.  */
  struct link_map *map;         /* Set to NULL by dlclose.  */
  void *eh_frame;
#if DLFO_STRUCT_HAS_EH_DBASE
  void *eh_dbase;
#endif
#if DLFO_STRUCT_HAS_EH_COUNT
  int eh_count;
#endif
};

/* Create a copy of *SOURCE in *COPY using relaxed MO loads and
   stores.  */
static inline void
_dl_find_object_internal_copy (const struct dl_find_object_internal *source,
                               struct dl_find_object_internal *copy)
{
  atomic_store_relaxed (&copy->map_start,
                        atomic_load_relaxed (&source->map_start));
  atomic_store_relaxed (&copy->map_end,
                        atomic_load_relaxed (&source->map_end));
  atomic_store_relaxed (&copy->map,
                        atomic_load_relaxed (&source->map));
  atomic_store_relaxed (&copy->eh_frame,
                        atomic_load_relaxed (&source->eh_frame));
#if DLFO_STRUCT_HAS_EH_DBASE
  atomic_store_relaxed (&copy->eh_dbase,
                        atomic_load_relaxed (&source->eh_dbase));
#endif
#if DLFO_STRUCT_HAS_EH_COUNT
  atomic_store_relaxed (&copy->eh_count,
                        atomic_load_relaxed (&source->eh_count));
#endif
}

static inline void
_dl_find_object_to_external (struct dl_find_object_internal *internal,
                             struct dl_find_object *external)
{
  external->dlfo_flags = 0;
  external->dlfo_map_start = (void *) internal->map_start;
  external->dlfo_map_end = (void *) internal->map_end;
  external->dlfo_link_map = internal->map;
  external->dlfo_eh_frame = internal->eh_frame;
# if DLFO_STRUCT_HAS_EH_DBASE
  external->dlfo_eh_dbase = internal->eh_dbase;
# endif
# if DLFO_STRUCT_HAS_EH_COUNT
  external->dlfo_eh_count = internal->eh_count;
# endif
}

/* Extract the object location data from a link map and writes it to
   *RESULT using relaxed MO stores.  */
static void __attribute__ ((unused))
_dl_find_object_from_map (struct link_map *l,
                          struct dl_find_object_internal *result)
{
  atomic_store_relaxed (&result->map_start, (uintptr_t) l->l_map_start);
  atomic_store_relaxed (&result->map_end, (uintptr_t) l->l_map_end);
  atomic_store_relaxed (&result->map, l);

#if DLFO_STRUCT_HAS_EH_DBASE
  atomic_store_relaxed (&result->eh_dbase, (void *) l->l_info[DT_PLTGOT]);
#endif

  for (const ElfW(Phdr) *ph = l->l_phdr, *ph_end = l->l_phdr + l->l_phnum;
       ph < ph_end; ++ph)
    if (ph->p_type == DLFO_EH_SEGMENT_TYPE)
      {
        atomic_store_relaxed (&result->eh_frame,
                              (void *) (ph->p_vaddr + l->l_addr));
#if DLFO_STRUCT_HAS_EH_COUNT
        atomic_store_relaxed (&result->eh_count, ph->p_memsz / 8);
#endif
        return;
      }

  /* Object has no exception handling segment.  */
  atomic_store_relaxed (&result->eh_frame, NULL);
#if DLFO_STRUCT_HAS_EH_COUNT
  atomic_store_relaxed (&result->eh_count, 0);
#endif
}

/* Called by the dynamic linker to set up the data structures for the
   initially loaded objects.  This creates a few persistent
   allocations, so it should be called with the minimal malloc.  */
void _dl_find_object_init (void) attribute_hidden;

/* Called by dlopen/dlmopen to add new objects to the DWARF EH frame
   data structures.  NEW_MAP is the dlopen'ed link map.  Link maps on
   the l_next list are added if l_object_processed is 0.  Needs to
   be protected by loader write lock.  Returns true on success, false
   on malloc failure.  */
bool _dl_find_object_update (struct link_map *new_map) attribute_hidden;

/* Called by dlclose to remove the link map from the DWARF EH frame
   data structures.  Needs to be protected by loader write lock.  */
void _dl_find_object_dlclose (struct link_map *l) attribute_hidden;

/* Called from __libc_freeres to deallocate malloc'ed memory.  */
void _dl_find_object_freeres (void) attribute_hidden;

#endif /* _DL_FIND_OBJECT_H */
