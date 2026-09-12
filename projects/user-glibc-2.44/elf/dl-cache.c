/* Support for reading /etc/ld.so.cache files written by Linux ldconfig.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
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
#include <intprops.h>
#include <unistd.h>
#include <ldsodefs.h>
#include <sys/mman.h>
#include <dl-cache.h>
#include <dl-scratch-buffer.h>
#include <stdint.h>
#include <_itoa.h>
#include <dl-hwcaps.h>
#include <dl-isa-level.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "tunconf.h"

/* This is the starting address and the size of the mmap()ed file.  */
static struct cache_file *cache;
static struct cache_file_new *cache_new;
static size_t cachesize;
static struct cache_extension_all_loaded ext;

static struct {
  typeof ((*(struct __stat64_t64 *)0).st_mtime) mtime;
  typeof ((*(struct __stat64_t64 *)0).st_ino) ino;
  typeof ((*(struct __stat64_t64 *)0).st_size) size;
  typeof ((*(struct __stat64_t64 *)0).st_dev) dev;
} cache_file_time, new_cache_file_time;

#ifdef SHARED
/* This is used to cache the priorities of glibc-hwcaps
   subdirectories.  The elements of _dl_cache_priorities correspond to
   the strings in the cache_extension_tag_glibc_hwcaps section.  */
static uint32_t *glibc_hwcaps_priorities;
static uint32_t glibc_hwcaps_priorities_length;
static uint32_t glibc_hwcaps_priorities_allocated;

/* True if the full malloc was used to allocated the array.  */
static bool glibc_hwcaps_priorities_malloced;

/* Deallocate the glibc_hwcaps_priorities array.  */
static void
glibc_hwcaps_priorities_free (void)
{
  /* When the minimal malloc is in use, free does not do anything,
     so it does not make sense to call it.  */
  if (glibc_hwcaps_priorities_malloced)
    free (glibc_hwcaps_priorities);
  glibc_hwcaps_priorities = NULL;
  glibc_hwcaps_priorities_allocated = 0;
  glibc_hwcaps_priorities_length = 0;
}

/* Ordered comparison of a hwcaps string from the cache on the left
   (identified by its string table index) and a _dl_hwcaps_priorities
   element on the right.  */
static int
glibc_hwcaps_compare (uint32_t left_index, struct dl_hwcaps_priority *right)
{
  const char *left_name = (const char *) cache + left_index;
  uint32_t left_name_length = strlen (left_name);
  uint32_t to_compare;
  if (left_name_length < right->name_length)
    to_compare = left_name_length;
  else
    to_compare = right->name_length;
  int cmp = memcmp (left_name, right->name, to_compare);
  if (cmp != 0)
    return cmp;
  if (left_name_length < right->name_length)
    return -1;
  else if (left_name_length > right->name_length)
    return 1;
  else
    return 0;
}

/* Initialize the glibc_hwcaps_priorities array and its length,
   glibc_hwcaps_priorities_length.  */
static void
glibc_hwcaps_priorities_init (void)
{
  uint32_t length = (ext.sections[cache_extension_tag_glibc_hwcaps].size
		     / sizeof (uint32_t));
  if (length > glibc_hwcaps_priorities_allocated)
    {
      glibc_hwcaps_priorities_free ();

      uint32_t *new_allocation = malloc (length * sizeof (uint32_t));
      if (new_allocation == NULL)
	/* This effectively disables hwcaps on memory allocation
	   errors.  */
	return;

      glibc_hwcaps_priorities = new_allocation;
      glibc_hwcaps_priorities_allocated = length;
      glibc_hwcaps_priorities_malloced = __rtld_malloc_is_complete ();
    }

  /* Compute the priorities for the subdirectories by merging the
     array in the cache with the dl_hwcaps_priorities array.  */
  const uint32_t *left = ext.sections[cache_extension_tag_glibc_hwcaps].base;
  const uint32_t *left_end = left + length;
  struct dl_hwcaps_priority *right = _dl_hwcaps_priorities;
  struct dl_hwcaps_priority *right_end = right + _dl_hwcaps_priorities_length;
  uint32_t *result = glibc_hwcaps_priorities;

  while (left < left_end && right < right_end)
    {
      if (*left < cachesize)
	{
	  int cmp = glibc_hwcaps_compare (*left, right);
	  if (cmp == 0)
	    {
	      *result = right->priority;
	      ++result;
	      ++left;
	      ++right;
	    }
	  else if (cmp < 0)
	    {
	      *result = 0;
	      ++result;
	      ++left;
	    }
	  else
	    ++right;
	}
      else
	{
	  *result = 0;
	  ++result;
	}
    }
  while (left < left_end)
    {
      *result = 0;
      ++result;
      ++left;
    }

  glibc_hwcaps_priorities_length = length;
}

/* Return the priority of the cache_extension_tag_glibc_hwcaps section
   entry at INDEX.  Zero means do not use.  Otherwise, lower values
   indicate greater preference.  */
static uint32_t
glibc_hwcaps_priority (uint32_t index)
{
  /* This does not need to repeated initialization attempts because
     this function is only called if there is glibc-hwcaps data in the
     cache, so the first call initializes the glibc_hwcaps_priorities
     array.  */
  if (glibc_hwcaps_priorities_length == 0)
    glibc_hwcaps_priorities_init ();

  if (index < glibc_hwcaps_priorities_length)
    return glibc_hwcaps_priorities[index];
  else
    return 0;
}
#endif /* SHARED */

/* True if PTR is a valid string table index.  */
static inline bool
_dl_cache_verify_ptr (uint32_t ptr, size_t string_table_size)
{
  return ptr < string_table_size;
}

/* Compute the address of the element INDEX of the array at LIBS.
   Conceptually, this is &LIBS[INDEX], but use ENTRY_SIZE for the size
   of *LIBS.  */
static inline const struct file_entry *
_dl_cache_file_entry (const struct file_entry *libs, size_t entry_size,
		      size_t index)
{
  return (const void *) libs + index * entry_size;
}

/* We use binary search since the table is sorted in the cache file.
   The first matching entry in the table is returned.  It is important
   to use the same algorithm as used while generating the cache file.
   STRING_TABLE_SIZE indicates the maximum offset in STRING_TABLE at
   which data is mapped; it is not exact.  */
static const char *
search_cache (const char *string_table, uint32_t string_table_size,
	      struct file_entry *libs, uint32_t nlibs, uint32_t entry_size,
	      const char *name)
{
  int left = 0;
  int right = nlibs - 1;
  const char *best = NULL;
#ifdef SHARED
  uint32_t best_priority = 0;
#endif

  while (left <= right)
    {
      int middle = (left + right) / 2;
      uint32_t key = _dl_cache_file_entry (libs, entry_size, middle)->key;

      /* Make sure string table indices are not bogus before using
	 them.  */
      if (!_dl_cache_verify_ptr (key, string_table_size))
	return NULL;

      /* Actually compare the entry with the key.  */
      int cmpres = _dl_cache_libcmp (name, string_table + key);
      if (__glibc_unlikely (cmpres == 0))
	{
	  /* Found it.  LEFT now marks the last entry for which we
	     know the name is correct.  */
	  left = middle;

	  /* There might be entries with this name before the one we
	     found.  So we have to find the beginning.  */
	  while (middle > 0)
	    {
	      key = _dl_cache_file_entry (libs, entry_size, middle - 1)->key;
	      /* Make sure string table indices are not bogus before
		 using them.  */
	      if (!_dl_cache_verify_ptr (key, string_table_size)
		  /* Actually compare the entry.  */
		  || _dl_cache_libcmp (name, string_table + key) != 0)
		break;
	      --middle;
	    }

	  do
	    {
	      int flags;
	      const struct file_entry *lib
		= _dl_cache_file_entry (libs, entry_size, middle);

	      /* Only perform the name test if necessary.  */
	      if (middle > left
		  /* We haven't seen this string so far.  Test whether the
		     index is ok and whether the name matches.  Otherwise
		     we are done.  */
		  && (! _dl_cache_verify_ptr (lib->key, string_table_size)
		      || (_dl_cache_libcmp (name, string_table + lib->key)
			  != 0)))
		break;

	      flags = lib->flags;
	      if (_dl_cache_check_flags (flags)
		  && _dl_cache_verify_ptr (lib->value, string_table_size))
		{
		  /* Named/extension hwcaps get slightly different
		     treatment: We keep searching for a better
		     match.  */
		  bool named_hwcap = false;

		  if (entry_size >= sizeof (struct file_entry_new))
		    {
		      /* The entry is large enough to include
			 HWCAP data.  Check it.  */
		      struct file_entry_new *libnew
			= (struct file_entry_new *) lib;

#ifdef SHARED
		      named_hwcap = dl_cache_hwcap_extension (libnew);
		      if (named_hwcap
			  && !dl_cache_hwcap_isa_level_compatible (libnew))
			continue;
#endif

		      /* The entries with named/extension hwcaps have
			 been exhausted (they are listed before all
			 other entries).  Return the best match
			 encountered so far if there is one.  */
		      if (!named_hwcap && best != NULL)
			break;

		      /* Skip entries with the legacy hwcap/platform mechanism
			 which was removed with glibc 2.37.  */
		      if (!named_hwcap && libnew->hwcap != 0)
			continue;

#ifdef SHARED
		      /* For named hwcaps, determine the priority and
			 see if beats what has been found so far.  */
		      if (named_hwcap)
			{
			  uint32_t entry_priority
			    = glibc_hwcaps_priority (libnew->hwcap);
			  if (entry_priority == 0)
			    /* Not usable at all.  Skip.  */
			    continue;
			  else if (best == NULL
				   || entry_priority < best_priority)
			    /* This entry is of higher priority
			       than the previous one, or it is the
			       first entry.  */
			    best_priority = entry_priority;
			  else
			    /* An entry has already been found,
			       but it is a better match.  */
			    continue;
			}
#endif /* SHARED */
		    }

		  best = string_table + lib->value;

		  if (!named_hwcap && flags == _DL_CACHE_DEFAULT_ID)
		    /* With named hwcaps, we need to keep searching to
		       see if we find a better match.  A better match
		       is also possible if the flags of the current
		       entry do not match the expected cache flags.
		       But if the flags match, no better entry will be
		       found.  */
		    break;
		}
	    }
	  while (++middle <= right);
	  break;
	}

      if (cmpres < 0)
	left = middle + 1;
      else
	right = middle - 1;
    }

  return best;
}

int
_dl_cache_libcmp (const char *p1, const char *p2)
{
  while (*p1 != '\0')
    {
      if (*p1 >= '0' && *p1 <= '9')
        {
          if (*p2 >= '0' && *p2 <= '9')
            {
	      /* Must compare this numerically.  */
	      int val1;
	      int val2;

	      val1 = *p1++ - '0';
	      val2 = *p2++ - '0';
	      while (*p1 >= '0' && *p1 <= '9')
	        val1 = val1 * 10 + *p1++ - '0';
	      while (*p2 >= '0' && *p2 <= '9')
	        val2 = val2 * 10 + *p2++ - '0';
	      if (val1 != val2)
		return val1 - val2;
	    }
	  else
            return 1;
        }
      else if (*p2 >= '0' && *p2 <= '9')
        return -1;
      else if (*p1 != *p2)
        return *p1 - *p2;
      else
	{
	  ++p1;
	  ++p2;
	}
    }
  return *p1 - *p2;
}

/* Set the cache back to the "no cache" state, which may include
   cleaning up a loaded cache.  */
static void
_dl_maybe_unload_ldsocache (void)
{
  if (cache != NULL)
    __munmap (cache, cachesize);

  cache = NULL;
  cache_new = NULL;
  cachesize = 0;

#ifdef SHARED
  glibc_hwcaps_priorities_free ();
#endif
}

/* Returns TRUE if for any reason the cache needs to be reloaded
   (including, the first time, loaded).  */
static bool
_dl_check_ldsocache_needs_loading (void)
{
  int rv;
  static bool copy_old_time = 0;
  struct __stat64_t64 new_cache_file_stat;

  /* Save the previous stat every time.  We only care when this
     changes, and we only stat it here, so we can get away with doing
     the copy now instead of at every single return statement in this
     function.  However, we only need to copy it if the previous stat
     succeeded.  The only way this could be subverted is if the admin
     moves the file aside, then moves it back, but CACHE would be set
     to NULL in the interim so that would be detected.  */
  if (copy_old_time)
    cache_file_time = new_cache_file_time;
  rv = __fstatat64_time64 (AT_FDCWD, LD_SO_CACHE, &new_cache_file_stat, 0);
  copy_old_time = (rv >= 0);

  /* No file to load, but there used to be.  Assume user intentionally
     deleted the cache and act accordingly.  */
  if (rv < 0 && cache != NULL)
    {
      _dl_maybe_unload_ldsocache ();
      return false;
    }

  /* No file to load and no loaded cache, so nothing to do.  */
  if (rv < 0)
    return false;

  /* Store the fields we check, in order they're likely to differ.  We
     must do this even for the first load (CACHE == NULL below), so that
     the next call copies an accurate NEW_CACHE_FILE_TIME into
     CACHE_FILE_TIME and does not spuriously reload the unchanged cache.  */
  new_cache_file_time.mtime = new_cache_file_stat.st_mtime;
  new_cache_file_time.ino = new_cache_file_stat.st_ino;
  new_cache_file_time.size = new_cache_file_stat.st_size;
  new_cache_file_time.dev = new_cache_file_stat.st_dev;

  /* Any file is better than no file (likely the first time
     through).  */
  if (cache == NULL)
    return true;

  /* At this point, NEW_CACHE_FILE_TIME is valid as well as
     CACHE_FILE_TIME, so we compare them.  */
  return (memcmp (&new_cache_file_time, &cache_file_time,
		  sizeof(new_cache_file_time)));
}

/* Attempts to load and validate the cache.  On return, CACHE is either
   unchanged (still loaded or still not loaded) or valid.  */
static void
_dl_maybe_load_ldsocache (void)
{
  struct cache_file *tmp_cache = NULL;
  struct cache_file_new *tmp_cache_new = NULL;
  size_t tmp_cachesize = 0;

  /* Read the contents of the file.  */
  void *file = _dl_sysdep_read_whole_file (LD_SO_CACHE, &tmp_cachesize,
					   PROT_READ);

  /* We can handle three different cache file formats here:
     - only the new format
     - the old libc5/glibc2.0/2.1 format
     - the old format with the new format in it
     The following checks if the cache contains any of these formats.  */
  if (file != MAP_FAILED && tmp_cachesize > sizeof *cache_new
      && memcmp (file, CACHEMAGIC_VERSION_NEW,
		 sizeof CACHEMAGIC_VERSION_NEW - 1) == 0
      /* Check for corruption, avoiding overflow.  */
      && ((tmp_cachesize - sizeof *cache_new) / sizeof (struct file_entry_new)
	  >= ((struct cache_file_new *) file)->nlibs))
    {
      if (! cache_file_new_matches_endian (file))
	{
	  __munmap (file, tmp_cachesize);
	  return;
	}

      tmp_cache_new = file;
      tmp_cache = file;
    }
  else if (file != MAP_FAILED && tmp_cachesize > sizeof *cache
	   && memcmp (file, CACHEMAGIC, sizeof CACHEMAGIC - 1) == 0
	   /* Check for corruption, avoiding overflow.  */
	   && ((tmp_cachesize - sizeof *cache) / sizeof (struct file_entry)
	       >= ((struct cache_file *) file)->nlibs))
    {
      size_t offset;
      /* Looks ok.  */
      tmp_cache = file;

      /* Check for new version.  */
      offset = ALIGN_CACHE (sizeof (struct cache_file)
			    + tmp_cache->nlibs * sizeof (struct file_entry));

      tmp_cache_new = (struct cache_file_new *) ((void *) tmp_cache + offset);
      if (tmp_cachesize < (offset + sizeof (struct cache_file_new))
	  || memcmp (tmp_cache_new->magic, CACHEMAGIC_VERSION_NEW,
		     sizeof CACHEMAGIC_VERSION_NEW - 1) != 0)
	tmp_cache_new = NULL;
      else
	{
	  if (! cache_file_new_matches_endian (tmp_cache_new))
	    /* The old-format part of the cache is bogus as well
	       if the endianness does not match.  (But it is
	       unclear how the new header can be located if the
	       endianness does not match.)  */
	    {
	      __munmap (file, tmp_cachesize);
	      return;
	    }
	}
    }
  else
    {
      if (file != MAP_FAILED)
	__munmap (file, tmp_cachesize);
      return;
    }

  struct cache_extension_all_loaded tmp_ext;
  if (!cache_extension_load (tmp_cache_new, tmp_cache, tmp_cachesize, &tmp_ext))
    {
      /* The extension is corrupt, so the cache is corrupt.  */
      __munmap (file, tmp_cachesize);
      return;
    }

  /* If we've gotten here, the loaded cache is good and we need to
     save it.  */
  _dl_maybe_unload_ldsocache ();
  cache = tmp_cache;
  cache_new = tmp_cache_new;
  cachesize = tmp_cachesize;
  ext = tmp_ext;

  assert (cache != NULL);
}


/* Look up NAME in ld.so.cache and return the file name stored there, or null
   if none is found.  The cache is loaded if it was not already.  If loading
   the cache previously failed there will be no more attempts to load it.
   The caller is responsible for freeing the returned string.  The ld.so.cache
   may be unmapped at any time by a completing recursive dlopen and
   this function must take care that it does not return references to
   any data in the mapping.  */
char *
_dl_load_cache_lookup (const char *name)
{
  /* Print a message if the loading of libs is traced.  */
  if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_LIBS))
    _dl_debug_printf (" search cache=%s\n", LD_SO_CACHE);

  if (_dl_check_ldsocache_needs_loading ())
    _dl_maybe_load_ldsocache ();

  if (cache == NULL)
    return NULL;

  const char *best;
  if (cache_new != NULL)
    {
      const char *string_table = (const char *) cache_new;
      best = search_cache (string_table, cachesize,
			   &cache_new->libs[0].entry, cache_new->nlibs,
			   sizeof (cache_new->libs[0]), name);
    }
  else
    {
      const char *string_table = (const char *) &cache->libs[cache->nlibs];
      uint32_t string_table_size
	= (const char *) cache + cachesize - string_table;
      best = search_cache (string_table, string_table_size,
			   &cache->libs[0], cache->nlibs,
			   sizeof (cache->libs[0]), name);
    }

  /* Print our result if wanted.  */
  if (__builtin_expect (GLRO(dl_debug_mask) & DL_DEBUG_LIBS, 0)
      && best != NULL)
    _dl_debug_printf ("  trying file=%s\n", best);

  if (best == NULL)
    return NULL;

  /* The double copy is *required* since malloc may be interposed
     and call dlopen itself whose completion would unmap the data
     we are accessing. Therefore we must make the copy of the
     mapping data without using malloc.  The DL_SCRATCH_NO_MALLOC
     forces any spill to anonymous mmap rather than the malloc.  */
  struct dl_scratch_buffer scratch = dl_scratch_buffer_init ();
  size_t best_len = strlen (best) + 1;
  dl_scratch_buffer_allocate (&scratch, best_len, DL_SCRATCH_NO_MALLOC);
  memcpy (scratch.data, best, best_len);
  char *result = __strdup (scratch.data);
  dl_scratch_buffer_free (&scratch);
  return result;
}

const struct tunable_header_cached *
_dl_load_cache_tunables (const char **data)
{
  struct cache_extension_all_loaded ext;
  struct tunable_header_cached *thc;
  struct tunable_entry_cached *tec;
  int i, count;

  if (_dl_check_ldsocache_needs_loading ())
    _dl_maybe_load_ldsocache ();

  if (cache_new)
    *data = (const char *) cache_new;
  else
    return NULL;

  if (!cache_extension_load (cache_new, cache, cachesize, &ext))
    return NULL;

  /* Validate length/contents here. */
  if (ext.sections[cache_extension_tag_tunables].size
      < sizeof(struct tunable_header_cached))
    return NULL;

  thc = (struct tunable_header_cached *)
    ext.sections[cache_extension_tag_tunables].base;

  /* Reject data produced by a different tunable cache format.  */
  if (thc->signature != TUNCONF_SIGNATURE || thc->version != TUNCONF_VERSION)
    return NULL;

  tec = thc->tunables;
  count = thc->num_tunables;

  if (ext.sections[cache_extension_tag_tunables].base
      + ext.sections[cache_extension_tag_tunables].size
      != (void *) & tec[count])
    return NULL;

  /* Validate each entry.  The string table lies between the file entries
     and the end of the mapping; clamp its end to CACHESIZE so that a bogus
     len_strings cannot make an offset point outside the mapped file.  */
  size_t s_start = (const char *) (&cache_new->libs[cache_new->nlibs]) - *data;
  size_t s_end;
  if (s_start >= cachesize
      || INT_ADD_WRAPV (s_start, cache_new->len_strings, &s_end))
    return NULL;
  if (s_end > cachesize)
    s_end = cachesize;
  for (i = 0; i < count; i ++)
    {
      if (thc->tunables[i].name_offset < s_start
	  || thc->tunables[i].name_offset >= s_end
	  || thc->tunables[i].value_offset < s_start
	  || thc->tunables[i].value_offset >= s_end)
	return NULL;
      if (thc->tunables[i].flag_offset != 0
	  && (thc->tunables[i].flag_offset < s_start
	      || thc->tunables[i].flag_offset >= s_end))
	return NULL;
    }

  return thc;
}
