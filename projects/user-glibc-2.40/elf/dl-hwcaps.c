/* Hardware capability support for run-time dynamic loader.
   Copyright (C) 2012-2024 Free Software Foundation, Inc.
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
#include <elf.h>
#include <errno.h>
#include <libintl.h>
#include <unistd.h>
#include <ldsodefs.h>

#include <dl-procinfo.h>
#include <dl-hwcaps.h>

/* This is the result of counting the substrings in a colon-separated
   hwcaps string.  */
struct hwcaps_counts
{
  /* Number of substrings.  */
  size_t count;

  /* Sum of the individual substring lengths (without separators or
     null terminators).  */
  size_t total_length;

  /* Maximum length of an individual substring.  */
  size_t maximum_length;
};

/* Update *COUNTS according to the contents of HWCAPS.  Skip over
   entries whose bit is not set in MASK.  */
static void
update_hwcaps_counts (struct hwcaps_counts *counts, const char *hwcaps,
		      uint32_t bitmask, const char *mask)
{
  struct dl_hwcaps_split_masked sp;
  _dl_hwcaps_split_masked_init (&sp, hwcaps, bitmask, mask);
  while (_dl_hwcaps_split_masked (&sp))
    {
      ++counts->count;
      counts->total_length += sp.split.length;
      if (sp.split.length > counts->maximum_length)
	counts->maximum_length = sp.split.length;
    }
}

/* State for copy_hwcaps.  Must be initialized to point to
   the storage areas for the array and the strings themselves.  */
struct copy_hwcaps
{
  struct r_strlenpair *next_pair;
  char *next_string;
};

/* Copy HWCAPS into the string pairs and strings, advancing *TARGET.
   Skip over entries whose bit is not set in MASK.  */
static void
copy_hwcaps (struct copy_hwcaps *target, const char *hwcaps,
	     uint32_t bitmask, const char *mask)
{
  struct dl_hwcaps_split_masked sp;
  _dl_hwcaps_split_masked_init (&sp, hwcaps, bitmask, mask);
  while (_dl_hwcaps_split_masked (&sp))
    {
      target->next_pair->str = target->next_string;
      char *slash = __mempcpy (__mempcpy (target->next_string,
					  GLIBC_HWCAPS_PREFIX,
					  strlen (GLIBC_HWCAPS_PREFIX)),
			       sp.split.segment, sp.split.length);
      *slash = '/';
      target->next_pair->len
	= strlen (GLIBC_HWCAPS_PREFIX) + sp.split.length + 1;
      ++target->next_pair;
      target->next_string = slash + 1;
    }
}

struct dl_hwcaps_priority *_dl_hwcaps_priorities;
uint32_t _dl_hwcaps_priorities_length;

/* Allocate _dl_hwcaps_priorities and fill it with data.  */
static void
compute_priorities (size_t total_count, const char *prepend,
		    uint32_t bitmask, const char *mask)
{
  _dl_hwcaps_priorities = malloc (total_count
				  * sizeof (*_dl_hwcaps_priorities));
  if (_dl_hwcaps_priorities == NULL)
    _dl_signal_error (ENOMEM, NULL, NULL,
		      N_("cannot create HWCAP priorities"));
  _dl_hwcaps_priorities_length = total_count;

  /* First the prepended subdirectories.  */
  size_t i = 0;
  {
    struct dl_hwcaps_split sp;
    _dl_hwcaps_split_init (&sp, prepend);
    while (_dl_hwcaps_split (&sp))
      {
	_dl_hwcaps_priorities[i].name = sp.segment;
	_dl_hwcaps_priorities[i].name_length = sp.length;
	_dl_hwcaps_priorities[i].priority = i + 1;
	++i;
      }
  }

  /* Then the built-in subdirectories that are actually active.  */
  {
    struct dl_hwcaps_split_masked sp;
    _dl_hwcaps_split_masked_init (&sp, _dl_hwcaps_subdirs, bitmask, mask);
    while (_dl_hwcaps_split_masked (&sp))
      {
	_dl_hwcaps_priorities[i].name = sp.split.segment;
	_dl_hwcaps_priorities[i].name_length = sp.split.length;
	_dl_hwcaps_priorities[i].priority = i + 1;
	++i;
      }
  }
  assert (i == total_count);
}

/* Sort the _dl_hwcaps_priorities array by name.  */
static void
sort_priorities_by_name (void)
{
  /* Insertion sort.  There is no need to link qsort into the dynamic
     loader for such a short array.  */
  for (size_t i = 1; i < _dl_hwcaps_priorities_length; ++i)
    for (size_t j = i; j > 0; --j)
      {
	struct dl_hwcaps_priority *previous = _dl_hwcaps_priorities + j - 1;
	struct dl_hwcaps_priority *current = _dl_hwcaps_priorities + j;

	/* Bail out if current is greater or equal to the previous
	   value.  */
	uint32_t to_compare;
	if (current->name_length < previous->name_length)
	  to_compare = current->name_length;
	else
	  to_compare = previous->name_length;
	int cmp = memcmp (current->name, previous->name, to_compare);
	if (cmp > 0
	    || (cmp == 0 && current->name_length >= previous->name_length))
	  break;

	/* Swap *previous and *current.  */
	struct dl_hwcaps_priority tmp = *previous;
	*previous = *current;
	*current = tmp;
      }
}

/* Return an array of useful/necessary hardware capability names.  */
const struct r_strlenpair *
_dl_important_hwcaps (const char *glibc_hwcaps_prepend,
		      const char *glibc_hwcaps_mask,
		      size_t *sz, size_t *max_capstrlen)
{
  /* glibc-hwcaps subdirectories.  */
  uint32_t hwcaps_subdirs_active = _dl_hwcaps_subdirs_active ();
  struct hwcaps_counts hwcaps_counts =  { 0, };
  update_hwcaps_counts (&hwcaps_counts, glibc_hwcaps_prepend, -1, NULL);
  update_hwcaps_counts (&hwcaps_counts, _dl_hwcaps_subdirs,
			hwcaps_subdirs_active, glibc_hwcaps_mask);
  compute_priorities (hwcaps_counts.count, glibc_hwcaps_prepend,
		      hwcaps_subdirs_active, glibc_hwcaps_mask);
  sort_priorities_by_name ();

  /* Each hwcaps subdirectory has a GLIBC_HWCAPS_PREFIX string prefix
     and a "/" suffix once stored in the result.  */
  hwcaps_counts.maximum_length += strlen (GLIBC_HWCAPS_PREFIX) + 1;
  size_t total = (hwcaps_counts.count * (strlen (GLIBC_HWCAPS_PREFIX) + 1)
		  + hwcaps_counts.total_length);

  *sz = hwcaps_counts.count + 1;

  /* This is the overall result.  */
  struct r_strlenpair *overall_result
    = malloc (*sz * sizeof (*overall_result) + total);
  if (overall_result == NULL)
    _dl_signal_error (ENOMEM, NULL, NULL,
		      N_("cannot create capability list"));

  /* Fill in the glibc-hwcaps subdirectories.  */
  {
    struct copy_hwcaps target;
    target.next_pair = overall_result;
    target.next_string = (char *) (overall_result + *sz);
    copy_hwcaps (&target, glibc_hwcaps_prepend, -1, NULL);
    copy_hwcaps (&target, _dl_hwcaps_subdirs,
		 hwcaps_subdirs_active, glibc_hwcaps_mask);

    /* Append an empty entry for the base directory itself.  */
    target.next_pair->str = target.next_string;
    target.next_pair->len = 0;
  }

  /* The maximum string length.  */
  *max_capstrlen = hwcaps_counts.maximum_length;

  return overall_result;
}
