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

#include <assert.h>
#include <atomic.h>
#include <atomic_wide_counter.h>
#include <dl-find_object.h>
#include <dlfcn.h>
#include <ldsodefs.h>
#include <link.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fallback implementation of _dl_find_object.  It uses a linear
   search, needs locking, and is not async-signal-safe.  It is used in
   _dl_find_object prior to initialization, when called from audit
   modules.  It also serves as the reference implementation for
   _dl_find_object.  */
static int
_dl_find_object_slow (void *pc, struct dl_find_object *result)
{
  ElfW(Addr) addr = (ElfW(Addr)) pc;
  for (Lmid_t ns = 0; ns < GL(dl_nns); ++ns)
    for (struct link_map *l = GL(dl_ns)[ns]._ns_loaded; l != NULL;
         l = l->l_next)
      if (addr >= l->l_map_start && addr < l->l_map_end
          && (l->l_contiguous || _dl_addr_inside_object (l, addr)))
        {
          assert (ns == l->l_ns);
          struct dl_find_object_internal internal;
          _dl_find_object_from_map (l, &internal);
          _dl_find_object_to_external (&internal, result);
          return 0;
        }

  /* Object not found.  */
  return -1;
}

/* Data for the main executable.  There is usually a large gap between
   the main executable and initially loaded shared objects.  Record
   the main executable separately, to increase the chance that the
   range for the non-closeable mappings below covers only the shared
   objects (and not also the gap between main executable and shared
   objects).  */
static struct dl_find_object_internal _dlfo_main attribute_relro;

/* Data for initially loaded shared objects that cannot be unloaded.
   (This may also contain non-contiguous mappings from the main
   executable.)  The mappings are stored in address order in the
   _dlfo_nodelete_mappings array (containing
   _dlfo_nodelete_mappings_size elements).  It is not modified after
   initialization.  */
static uintptr_t _dlfo_nodelete_mappings_end attribute_relro;
static size_t _dlfo_nodelete_mappings_size attribute_relro;
static struct dl_find_object_internal *_dlfo_nodelete_mappings
  attribute_relro;

/* Mappings created by dlopen can go away with dlclose, so a dynamic
   data structure with some synchronization is needed.  Individual
   segments are similar to the _dlfo_nodelete_mappings array above.
   The previous segment contains lower addresses and is at most half
   as long.  Checking the address of the base address of the first
   element during a lookup can therefore approximate a binary search
   over all segments, even though the data is not stored in one
   contiguous array.

   During updates, the segments are overwritten in place.  A software
   transactional memory construct (involving the
   _dlfo_loaded_mappings_version variable) is used to detect
   concurrent modification, and retry as necessary.  (This approach is
   similar to seqlocks, except that two copies are used, and there is
   only one writer, ever, due to the loader lock.)  Technically,
   relaxed MO loads and stores need to be used for the shared TM data,
   to avoid data races.

   The memory allocations are never deallocated, but slots used for
   objects that have been dlclose'd can be reused by dlopen.  The
   memory can live in the regular C malloc heap.

   The segments are populated from the start of the list, with the
   mappings with the highest address.  Only if this segment is full,
   previous segments are used for mappings at lower addresses.  The
   remaining segments are populated as needed, but after allocating
   further segments, some of the initial segments (at the end of the
   linked list) can be empty (with size 0).

   Adding new elements to this data structure is another source of
   quadratic behavior for dlopen.  If the other causes of quadratic
   behavior are eliminated, a more complicated data structure will be
   needed.  */
struct dlfo_mappings_segment
{
  /* The previous segment has lower base addresses.  Constant after
     initialization; read in the TM region.  */
  struct dlfo_mappings_segment *previous;

  /* Used by __libc_freeres to deallocate malloc'ed memory.  */
  void *to_free;

  /* Count of array elements in use and allocated.  */
  size_t size;                  /* Read in the TM region.  */
  size_t allocated;

  struct dl_find_object_internal objects[]; /* Read in the TM region.  */
};

/* To achieve async-signal-safety, two copies of the data structure
   are used, so that a signal handler can still use this data even if
   dlopen or dlclose modify the other copy.  The the least significant
   bit in _dlfo_loaded_mappings_version determines which array element
   is the currently active region.  */
static struct dlfo_mappings_segment *_dlfo_loaded_mappings[2];

/* Returns the number of actually used elements in all segments
   starting at SEG.  */
static inline size_t
_dlfo_mappings_segment_count_used (struct dlfo_mappings_segment *seg)
{
  size_t count = 0;
  for (; seg != NULL && seg->size > 0; seg = seg->previous)
    for (size_t i = 0; i < seg->size; ++i)
      /* Exclude elements which have been dlclose'd.  */
      count += seg->objects[i].map != NULL;
  return count;
}

/* Compute the total number of available allocated segments linked
   from SEG.  */
static inline size_t
_dlfo_mappings_segment_count_allocated (struct dlfo_mappings_segment *seg)
{
  size_t count = 0;
  for (; seg != NULL; seg = seg->previous)
    count += seg->allocated;
  return count;
}

/* This is essentially an arbitrary value.  dlopen allocates plenty of
   memory anyway, so over-allocated a bit does not hurt.  Not having
   many small-ish segments helps to avoid many small binary searches.
   Not using a power of 2 means that we do not waste an extra page
   just for the malloc header if a mapped allocation is used in the
   glibc allocator.  */
enum { dlfo_mappings_initial_segment_size = 63 };

/* Allocate an empty segment.  This used for the first ever
   allocation.  */
static struct dlfo_mappings_segment *
_dlfo_mappings_segment_allocate_unpadded (size_t size)
{
  if (size < dlfo_mappings_initial_segment_size)
    size = dlfo_mappings_initial_segment_size;
  /* No overflow checks here because the size is a mapping count, and
     struct link_map is larger than what we allocate here.  */
  enum
    {
      element_size = sizeof ((struct dlfo_mappings_segment) {}.objects[0])
    };
  size_t to_allocate = (sizeof (struct dlfo_mappings_segment)
                        + size * element_size);
  struct dlfo_mappings_segment *result = malloc (to_allocate);
  if (result != NULL)
    {
      result->previous = NULL;
      result->to_free = NULL; /* Minimal malloc memory cannot be freed.  */
      result->size = 0;
      result->allocated = size;
    }
  return result;
}

/* Allocate an empty segment that is at least SIZE large.  PREVIOUS
   points to the chain of previously allocated segments and can be
   NULL.  */
static struct dlfo_mappings_segment *
_dlfo_mappings_segment_allocate (size_t size,
                                 struct dlfo_mappings_segment * previous)
{
  /* Exponential sizing policies, so that lookup approximates a binary
     search.  */
  {
    size_t minimum_growth;
    if (previous == NULL)
      minimum_growth = dlfo_mappings_initial_segment_size;
    else
      minimum_growth = 2* previous->allocated;
    if (size < minimum_growth)
      size = minimum_growth;
  }
  enum { cache_line_size_estimate = 128 };
  /* No overflow checks here because the size is a mapping count, and
     struct link_map is larger than what we allocate here.  */
  enum
    {
      element_size = sizeof ((struct dlfo_mappings_segment) {}.objects[0])
    };
  size_t to_allocate = (sizeof (struct dlfo_mappings_segment)
                        + size * element_size
                        + 2 * cache_line_size_estimate);
  char *ptr = malloc (to_allocate);
  if (ptr == NULL)
    return NULL;
  char *original_ptr = ptr;
  /* Start and end at a (conservative) 128-byte cache line boundary.
     Do not use memalign for compatibility with partially interposing
     malloc implementations.  */
  char *end = PTR_ALIGN_DOWN (ptr + to_allocate, cache_line_size_estimate);
  ptr = PTR_ALIGN_UP (ptr, cache_line_size_estimate);
  struct dlfo_mappings_segment *result
    = (struct dlfo_mappings_segment *) ptr;
  result->previous = previous;
  result->to_free = original_ptr;
  result->size = 0;
  /* We may have obtained slightly more space if malloc happened
     to provide an over-aligned pointer.  */
  result->allocated = (((uintptr_t) (end - ptr)
                        - sizeof (struct dlfo_mappings_segment))
                       / element_size);
  assert (result->allocated >= size);
  return result;
}

/* Monotonic counter for software transactional memory.  The lowest
   bit indicates which element of the _dlfo_loaded_mappings contains
   up-to-date data.  */
static __atomic_wide_counter _dlfo_loaded_mappings_version;

/* TM version at the start of the read operation.  */
static inline uint64_t
_dlfo_read_start_version (void)
{
  /* Acquire MO load synchronizes with the fences at the beginning and
     end of the TM update region in _dlfo_mappings_begin_update,
     _dlfo_mappings_end_update.  */
  return __atomic_wide_counter_load_acquire (&_dlfo_loaded_mappings_version);
}

/* Optimized variant of _dlfo_read_start_version which can be called
   when the loader is write-locked.  */
static inline uint64_t
_dlfo_read_version_locked (void)
{
  return __atomic_wide_counter_load_relaxed (&_dlfo_loaded_mappings_version);
}

/* Update the version to reflect that an update is happening.  This
   does not change the bit that controls the active segment chain.  */
static inline void
_dlfo_mappings_begin_update (void)
{
  /* The fence synchronizes with loads in _dlfo_read_start_version
     (also called from _dlfo_read_success).  */
  atomic_thread_fence_release ();
}

/* Installs the just-updated version as the active version.  */
static inline void
_dlfo_mappings_end_update (void)
{
  /* The fence synchronizes with loads in _dlfo_read_start_version
     (also called from _dlfo_read_success).  */
  atomic_thread_fence_release ();
  /* No atomic read-modify-write update needed because of the loader
     lock.  */
  __atomic_wide_counter_add_relaxed (&_dlfo_loaded_mappings_version, 1);
}

/* Return true if the read was successful, given the start
   version.  */
static inline bool
_dlfo_read_success (uint64_t start_version)
{
  /* See Hans Boehm, Can Seqlocks Get Along with Programming Language
     Memory Models?, Section 4.  This is necessary so that loads in
     the TM region are not ordered past the version check below.  */
  atomic_thread_fence_acquire ();

  /* Synchronizes with the fences in _dlfo_mappings_begin_update,
     _dlfo_mappings_end_update.  It is important that all stores from
     the last update have become visible, and stores from the next
     update to this version are not before the version number is
     updated.

     Unlike with seqlocks, there is no check for odd versions here
     because we have read the unmodified copy (confirmed to be
     unmodified by the unchanged version).  */
  return _dlfo_read_start_version () == start_version;
}

/* Returns the active segment identified by the specified start
   version.  */
static struct dlfo_mappings_segment *
_dlfo_mappings_active_segment (uint64_t start_version)
{
  return _dlfo_loaded_mappings[start_version & 1];
}

/* Searches PC among the address-sorted array [FIRST1, FIRST1 +
   SIZE).  Assumes PC >= FIRST1->map_start.  Returns a pointer to the
   element that contains PC, or NULL if there is no such element.  */
static inline struct dl_find_object_internal *
_dlfo_lookup (uintptr_t pc, struct dl_find_object_internal *first1, size_t size)
{
  struct dl_find_object_internal *end = first1 + size;

  /* Search for a lower bound in first.  */
  struct dl_find_object_internal *first = first1;
  while (size > 0)
    {
      size_t half = size >> 1;
      struct dl_find_object_internal *middle = first + half;
      if (atomic_load_relaxed (&middle->map_start) < pc)
        {
          first = middle + 1;
          size -= half + 1;
        }
      else
        size = half;
    }

  if (first != end && pc == atomic_load_relaxed (&first->map_start))
    {
      if (pc < atomic_load_relaxed (&first->map_end))
        return first;
      else
        /* Zero-length mapping after dlclose.  */
        return NULL;
    }
  else
    {
      /* Check to see if PC is in the previous mapping.  */
      --first;
      if (pc < atomic_load_relaxed (&first->map_end))
        /* pc >= first->map_start implied by the search above.  */
        return first;
      else
        return NULL;
    }
}

int
__dl_find_object (void *pc1, struct dl_find_object *result)
{
  uintptr_t pc = (uintptr_t) pc1;

  if (__glibc_unlikely (_dlfo_main.map_end == 0))
    {
      /* Not initialized.  No locking is needed here because this can
         only be called from audit modules, which cannot create
         threads.  */
      return _dl_find_object_slow (pc1, result);
    }

  /* Main executable.  */
  if (pc >= _dlfo_main.map_start && pc < _dlfo_main.map_end)
    {
      _dl_find_object_to_external (&_dlfo_main, result);
      return 0;
    }

  /* Other initially loaded objects.  */
  if (pc >= _dlfo_nodelete_mappings->map_start
      && pc < _dlfo_nodelete_mappings_end)
    {
      struct dl_find_object_internal *obj
        = _dlfo_lookup (pc, _dlfo_nodelete_mappings,
                        _dlfo_nodelete_mappings_size);
      if (obj != NULL)
        {
          _dl_find_object_to_external (obj, result);
          return 0;
        }
      /* Fall through to the full search.  The kernel may have mapped
         the initial mappings with gaps that are later filled by
         dlopen with other mappings.  */
    }

  /* Handle audit modules, dlopen, dlopen objects.  This uses software
     transactional memory, with a retry loop in case the version
     changes during execution.  */
  while (true)
    {
    retry:
      ;
      uint64_t start_version = _dlfo_read_start_version ();

      /* The read through seg->previous assumes that the CPU
         recognizes the load dependency, so that no invalid size
         values is read.  Furthermore, the code assumes that no
         out-of-thin-air value for seg->size is observed.  Together,
         this ensures that the observed seg->size value is always less
         than seg->allocated, so that _dlfo_mappings_index does not
         read out-of-bounds.  (This avoids intermediate TM version
         verification.  A concurrent version update will lead to
         invalid lookup results, but not to out-of-memory access.)

         Either seg == NULL or seg->size == 0 terminates the segment
         list.  _dl_find_object_update does not bother to clear the
         size on earlier unused segments.  */
      for (struct dlfo_mappings_segment *seg
             = _dlfo_mappings_active_segment (start_version);
           seg != NULL;
           seg = atomic_load_acquire (&seg->previous))
        {
          size_t seg_size = atomic_load_relaxed (&seg->size);
          if (seg_size == 0)
            break;

          if (pc >= atomic_load_relaxed (&seg->objects[0].map_start))
            {
              /* PC may lie within this segment.  If it is less than the
                 segment start address, it can only lie in a previous
                 segment, due to the base address sorting.  */
              struct dl_find_object_internal *obj
                = _dlfo_lookup (pc, seg->objects, seg_size);

              if (obj != NULL)
                {
                  /* Found the right mapping.  Copy out the data prior to
                     checking if the read transaction was successful.  */
                  struct dl_find_object_internal copy;
                  _dl_find_object_internal_copy (obj, &copy);
                  if (_dlfo_read_success (start_version))
                    {
                      _dl_find_object_to_external (&copy, result);
                      return 0;
                    }
                  else
                    /* Read transaction failure.  */
                    goto retry;
                }
              else
                {
                  /* PC is not covered by this mapping.  */
                  if (_dlfo_read_success (start_version))
                    return -1;
                  else
                    /* Read transaction failure.  */
                    goto retry;
                }
            } /* if: PC might lie within the current seg.  */
        }

      /* PC is not covered by any segment.  */
      if (_dlfo_read_success (start_version))
        return -1;
    } /* Transaction retry loop.  */
}
hidden_def (__dl_find_object)
weak_alias (__dl_find_object, _dl_find_object)

/* _dlfo_process_initial is called twice.  First to compute the array
   sizes from the initial loaded mappings.  Second to fill in the
   bases and infos arrays with the (still unsorted) data.  Returns the
   number of loaded (non-nodelete) mappings.  */
static size_t
_dlfo_process_initial (void)
{
  struct link_map *main_map = GL(dl_ns)[LM_ID_BASE]._ns_loaded;

  size_t nodelete = 0;
  if (!main_map->l_contiguous)
    {
      struct dl_find_object_internal dlfo;
      _dl_find_object_from_map (main_map, &dlfo);

      /* PT_LOAD segments for a non-contiguous are added to the
         non-closeable mappings.  */
      for (const ElfW(Phdr) *ph = main_map->l_phdr,
             *ph_end = main_map->l_phdr + main_map->l_phnum;
           ph < ph_end; ++ph)
        if (ph->p_type == PT_LOAD)
          {
            if (_dlfo_nodelete_mappings != NULL)
              {
                /* Second pass only.  */
                _dlfo_nodelete_mappings[nodelete] = dlfo;
                _dlfo_nodelete_mappings[nodelete].map_start
                  = ph->p_vaddr + main_map->l_addr;
                _dlfo_nodelete_mappings[nodelete].map_end
                  = _dlfo_nodelete_mappings[nodelete].map_start + ph->p_memsz;
              }
            ++nodelete;
          }
    }

  size_t loaded = 0;
  for (Lmid_t ns = 0; ns < GL(dl_nns); ++ns)
    for (struct link_map *l = GL(dl_ns)[ns]._ns_loaded; l != NULL;
         l = l->l_next)
      /* Skip the main map processed above, and proxy maps.  */
      if (l != main_map && l == l->l_real)
        {
          /* lt_library link maps are implicitly NODELETE.  */
          if (l->l_type == lt_library || l->l_nodelete_active)
            {
              if (_dlfo_nodelete_mappings != NULL)
                /* Second pass only.  */
                _dl_find_object_from_map
                  (l, _dlfo_nodelete_mappings + nodelete);
              ++nodelete;
            }
          else if (l->l_type == lt_loaded)
            {
              if (_dlfo_loaded_mappings[0] != NULL)
                /* Second pass only.  */
                _dl_find_object_from_map
                  (l, &_dlfo_loaded_mappings[0]->objects[loaded]);
              ++loaded;
            }
        }

  _dlfo_nodelete_mappings_size = nodelete;
  return loaded;
}

/* Selection sort based on mapping start address.  */
void
_dlfo_sort_mappings (struct dl_find_object_internal *objects, size_t size)
{
  if (size < 2)
    return;

  for (size_t i = 0; i < size - 1; ++i)
    {
      /* Find minimum.  */
      size_t min_idx = i;
      uintptr_t min_val = objects[i].map_start;
      for (size_t j = i + 1; j < size; ++j)
        if (objects[j].map_start < min_val)
          {
            min_idx = j;
            min_val = objects[j].map_start;
          }

      /* Swap into place.  */
      struct dl_find_object_internal tmp = objects[min_idx];
      objects[min_idx] = objects[i];
      objects[i] = tmp;
    }
}

void
_dl_find_object_init (void)
{
  /* Cover the main mapping.  */
  {
    struct link_map *main_map = GL(dl_ns)[LM_ID_BASE]._ns_loaded;

    if (main_map->l_contiguous)
      _dl_find_object_from_map (main_map, &_dlfo_main);
    else
      {
        /* Non-contiguous main maps are handled in
           _dlfo_process_initial.  Mark as initialized, but not
           coverying any valid PC.  */
        _dlfo_main.map_start = -1;
        _dlfo_main.map_end = -1;
      }
  }

  /* Allocate the data structures.  */
  size_t loaded_size = _dlfo_process_initial ();
  _dlfo_nodelete_mappings = malloc (_dlfo_nodelete_mappings_size
                                    * sizeof (*_dlfo_nodelete_mappings));
  if (loaded_size > 0)
    _dlfo_loaded_mappings[0]
      = _dlfo_mappings_segment_allocate_unpadded (loaded_size);
  if (_dlfo_nodelete_mappings == NULL
      || (loaded_size > 0 && _dlfo_loaded_mappings[0] == NULL))
    _dl_fatal_printf ("\
Fatal glibc error: cannot allocate memory for find-object data\n");
  /* Fill in the data with the second call.  */
  _dlfo_nodelete_mappings_size = 0;
  _dlfo_process_initial ();

  /* Sort both arrays.  */
  if (_dlfo_nodelete_mappings_size > 0)
    {
      _dlfo_sort_mappings (_dlfo_nodelete_mappings,
                           _dlfo_nodelete_mappings_size);
      size_t last_idx = _dlfo_nodelete_mappings_size - 1;
      _dlfo_nodelete_mappings_end = _dlfo_nodelete_mappings[last_idx].map_end;
    }
  if (loaded_size > 0)
    _dlfo_sort_mappings (_dlfo_loaded_mappings[0]->objects,
                         _dlfo_loaded_mappings[0]->size);
}

static void
_dl_find_object_link_map_sort (struct link_map **loaded, size_t size)
{
  /* Selection sort based on map_start.  */
  if (size < 2)
    return;
  for (size_t i = 0; i < size - 1; ++i)
    {
      /* Find minimum.  */
      size_t min_idx = i;
      ElfW(Addr) min_val = loaded[i]->l_map_start;
      for (size_t j = i + 1; j < size; ++j)
        if (loaded[j]->l_map_start < min_val)
          {
            min_idx = j;
            min_val = loaded[j]->l_map_start;
          }

      /* Swap into place.  */
      struct link_map *tmp = loaded[min_idx];
      loaded[min_idx] = loaded[i];
      loaded[i] = tmp;
    }
}

/* Initializes the segment for writing.  Returns the target write
   index (plus 1) in this segment.  The index is chosen so that a
   partially filled segment still has data at index 0.  */
static inline size_t
_dlfo_update_init_seg (struct dlfo_mappings_segment *seg,
                       size_t remaining_to_add)
{
  size_t new_seg_size;
  if (remaining_to_add < seg->allocated)
    /* Partially filled segment.  */
    new_seg_size = remaining_to_add;
  else
    new_seg_size = seg->allocated;
  atomic_store_relaxed (&seg->size, new_seg_size);
  return new_seg_size;
}

/* Invoked from _dl_find_object_update after sorting.  Stores to the
   shared data need to use relaxed MO.  But plain loads can be used
   because the loader lock prevents concurrent stores.  */
static bool
_dl_find_object_update_1 (struct link_map **loaded, size_t count)
{
  int active_idx = _dlfo_read_version_locked () & 1;

  struct dlfo_mappings_segment *current_seg
    = _dlfo_loaded_mappings[active_idx];
  size_t current_used = _dlfo_mappings_segment_count_used (current_seg);

  struct dlfo_mappings_segment *target_seg
    = _dlfo_loaded_mappings[!active_idx];
  size_t remaining_to_add = current_used + count;

  /* remaining_to_add can be 0 if (current_used + count) wraps, but in practice
     this is not possible as it represent counts of link maps.  Link maps have
     sizes larger than 1 byte, so the sum of any two link map counts will
     always fit within a size_t without wrapping around.  This check ensures
     that target_seg is not erroneously considered potentially NULL by GCC. */
  if (remaining_to_add == 0)
    __builtin_unreachable ();

  /* Ensure that the new segment chain has enough space.  */
  {
    size_t new_allocated
      = _dlfo_mappings_segment_count_allocated (target_seg);
    if (new_allocated < remaining_to_add)
      {
        size_t more = remaining_to_add - new_allocated;
        target_seg = _dlfo_mappings_segment_allocate (more, target_seg);
        if (target_seg == NULL)
          /* Out of memory.  Do not end the update and keep the
             current version unchanged.  */
          return false;

        /* Start update cycle. */
        _dlfo_mappings_begin_update ();

        /* The barrier ensures that a concurrent TM read or fork does
           not see a partially initialized segment.  */
        atomic_store_release (&_dlfo_loaded_mappings[!active_idx], target_seg);
      }
    else
      /* Start update cycle without allocation.  */
      _dlfo_mappings_begin_update ();
  }

  size_t target_seg_index1 = _dlfo_update_init_seg (target_seg,
                                                    remaining_to_add);

  /* Merge the current_seg segment list with the loaded array into the
     target_set.  Merging occurs backwards, in decreasing l_map_start
     order.  */
  size_t loaded_index1 = count;
  size_t current_seg_index1;
  if (current_seg == NULL)
    current_seg_index1 = 0;
  else
    current_seg_index1 = current_seg->size;
  while (true)
    {
      if (current_seg_index1 == 0)
        {
          /* Switch to the previous segment.  */
          if (current_seg != NULL)
            current_seg = current_seg->previous;
          if (current_seg != NULL)
            {
              current_seg_index1 = current_seg->size;
              if (current_seg_index1 == 0)
                /* No more data in previous segments.  */
                current_seg = NULL;
            }
        }

      if (current_seg != NULL
          && (current_seg->objects[current_seg_index1 - 1].map == NULL))
        {
          /* This mapping has been dlclose'd.  Do not copy it.  */
          --current_seg_index1;
          continue;
        }

      if (loaded_index1 == 0 && current_seg == NULL)
        /* No more data in either source.  */
        break;

      /* Make room for another mapping.  */
      assert (remaining_to_add > 0);
      if (target_seg_index1 == 0)
        {
          /* Switch segments and set the size of the segment.  */
          target_seg = target_seg->previous;
          target_seg_index1 = _dlfo_update_init_seg (target_seg,
                                                     remaining_to_add);
        }

      /* Determine where to store the data.  */
      struct dl_find_object_internal *dlfo
        = &target_seg->objects[target_seg_index1 - 1];

      if (loaded_index1 == 0
          || (current_seg != NULL
              && (loaded[loaded_index1 - 1]->l_map_start
                  < current_seg->objects[current_seg_index1 - 1].map_start)))
        {
          /* Prefer mapping in current_seg.  */
          assert (current_seg_index1 > 0);
          _dl_find_object_internal_copy
            (&current_seg->objects[current_seg_index1 - 1], dlfo);
          --current_seg_index1;
        }
      else
        {
          /* Prefer newly loaded link map.  */
          assert (loaded_index1 > 0);
          _dl_find_object_from_map (loaded[loaded_index1 - 1], dlfo);
          loaded[loaded_index1 -  1]->l_find_object_processed = 1;
          --loaded_index1;
        }

      /* Consume space in target segment.  */
      --target_seg_index1;

      --remaining_to_add;
    }

  /* Everything has been added.  */
  assert (remaining_to_add == 0);

  /* The segment must have been filled up to the beginning.  */
  assert (target_seg_index1 == 0);

  /* Prevent searching further into unused segments.  */
  if (target_seg->previous != NULL)
    atomic_store_relaxed (&target_seg->previous->size, 0);

  _dlfo_mappings_end_update ();
  return true;
}

bool
_dl_find_object_update (struct link_map *new_map)
{
  /* Copy the newly-loaded link maps into an array for sorting.  */
  size_t count = 0;
  for (struct link_map *l = new_map; l != NULL; l = l->l_next)
    /* Skip proxy maps and already-processed maps.  */
    count += l == l->l_real && !l->l_find_object_processed;
  if (count == 0)
    return true;

  struct link_map **map_array = malloc (count * sizeof (*map_array));
  if (map_array == NULL)
    return false;
  {
    size_t i = 0;
    for (struct link_map *l = new_map; l != NULL; l = l->l_next)
      if (l == l->l_real && !l->l_find_object_processed)
        map_array[i++] = l;
  }

  _dl_find_object_link_map_sort (map_array, count);
  bool ok = _dl_find_object_update_1 (map_array, count);
  free (map_array);
  return ok;
}

void
_dl_find_object_dlclose (struct link_map *map)
{
  uint64_t start_version = _dlfo_read_version_locked ();
  uintptr_t map_start = map->l_map_start;


  /* Directly patch the size information in the mapping to mark it as
     unused.  See the parallel lookup logic in _dl_find_object.  Do
     not check for previous dlclose at the same mapping address
     because that cannot happen (there would have to be an
     intermediate dlopen, which drops size-zero mappings).  */
  for (struct dlfo_mappings_segment *seg
         = _dlfo_mappings_active_segment (start_version);
       seg != NULL && seg->size > 0; seg = seg->previous)
    if (map_start >= seg->objects[0].map_start)
      {
        struct dl_find_object_internal *obj
          = _dlfo_lookup (map_start, seg->objects, seg->size);
        if (obj == NULL)
          /* Ignore missing link maps because of potential shutdown
             issues around __libc_freeres.  */
            return;

        /* Mark as closed.  This does not change the overall data
           structure, so no TM cycle is needed.  */
        atomic_store_relaxed (&obj->map_end, obj->map_start);
        atomic_store_relaxed (&obj->map, NULL);
        return;
      }
}

void
_dl_find_object_freeres (void)
{
  for (int idx = 0; idx < 2; ++idx)
    {
      for (struct dlfo_mappings_segment *seg = _dlfo_loaded_mappings[idx];
           seg != NULL; )
        {
          struct dlfo_mappings_segment *previous = seg->previous;
          free (seg->to_free);
          seg = previous;
        }
      /* Stop searching in shared objects.  */
      _dlfo_loaded_mappings[idx] = 0;
    }
}
