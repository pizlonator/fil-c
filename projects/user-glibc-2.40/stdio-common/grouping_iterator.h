/* Iterator for grouping a number while scanning it forward.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef GROUPING_ITERATOR_H
#define GROUPING_ITERATOR_H

#include <locale.h>
#include <stdbool.h>

struct grouping_iterator
{
  /* Number of characters in the current group.  If this reaches zero,
     a thousands separator needs to be emittted.  */
  unsigned int remaining_in_current_group;

  /* Number of characters remaining in the number.  This is used to
     detect the start of the non-repeating groups.  */
  unsigned int remaining;

  /* Points to the current grouping descriptor.  */
  const char *groupings;

  /* Total number of characters in the non-repeating groups.  */
  unsigned int non_repeating_groups;

  /* Number of separators that will be inserted if the whole number is
     processed.  (Does not change during iteration.)  */
  unsigned int separators;
};

struct __locale_data;

/* Initializes *IT with the data from LOCDATA (which must be for
   LC_MONETARY or LC_NUMERIC).  DIGITS is the length of the number.
   Returns true if grouping is active, false if not.  */
bool __grouping_iterator_init (struct grouping_iterator *it,
                               int category, locale_t loc,
                               unsigned int digits) attribute_hidden;

/* Initializes *IT with no grouping information for a string of length
   DIGITS, and return false to indicate no grouping.  */
bool __grouping_iterator_init_none (struct grouping_iterator *it,
                                    unsigned int digits)
  attribute_hidden;

/* Advances to the next character and returns true if a thousands
   separator should be inserted before emitting it.  */
bool __grouping_iterator_next (struct grouping_iterator *it);

#endif /* GROUPING_ITERATOR_H */
