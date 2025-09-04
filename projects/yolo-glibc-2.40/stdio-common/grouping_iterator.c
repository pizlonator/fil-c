/* Iterator for inserting thousands separators into numbers.
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

#include <grouping_iterator.h>

#include <assert.h>
#include <limits.h>
#include <locale/localeinfo.h>
#include <stdint.h>
#include <string.h>

/* Initializes *IT with no grouping information for a string of length
   DIGITS, and return false to indicate no grouping.  */
bool
__grouping_iterator_init_none (struct grouping_iterator *it,
                               unsigned int digits)
{
  memset (it, 0, sizeof (*it));
  it->remaining_in_current_group = digits;
  it->remaining = digits;
  return false;
}

static bool
grouping_iterator_setup (struct grouping_iterator *it, unsigned int digits,
                         const char *grouping)
{
  /* We treat all negative values like CHAR_MAX.  */

  if (*grouping == CHAR_MAX || *grouping <= 0)
    /* No grouping should be done.  */
    return __grouping_iterator_init_none (it, digits);

  unsigned int remaining_to_group = digits;
  unsigned int non_repeating_groups = 0;
  unsigned int groups = 0;
  while (true)
    {
      non_repeating_groups += *grouping;
      if (remaining_to_group <= (unsigned int) *grouping)
        break;

      ++groups;
      remaining_to_group -= *grouping++;

      if (*grouping == CHAR_MAX
#if CHAR_MIN < 0
          || *grouping < 0
#endif
          )
          /* No more grouping should be done.  */
        break;
      else if (*grouping == 0)
        {
          /* Same grouping repeats.  */
          --grouping;
          non_repeating_groups -= *grouping; /* Over-counted.  */
          unsigned int repeats = (remaining_to_group - 1) / *grouping;
          groups += repeats;
          remaining_to_group -= repeats * *grouping;
          break;
        }
    }

  it->remaining_in_current_group = remaining_to_group;
  it->remaining = digits;
  it->groupings = grouping;
  it->non_repeating_groups = non_repeating_groups;
  it->separators = groups;
  return it->separators > 0;
}

/* Returns the appropriate grouping item in LOC depending on CATEGORY
   (which must be LC_MONETARY or LC_NUMERIC).  */
static const char *
get_grouping (int category, locale_t loc)
{
  return _nl_lookup (loc, category,
                     category == LC_MONETARY ? MON_GROUPING : GROUPING);
}


bool
__grouping_iterator_init (struct grouping_iterator *it,
                          int category, locale_t loc, unsigned int digits)
{
  if (digits <= 1)
    return __grouping_iterator_init_none (it, digits);
  else
    return grouping_iterator_setup (it, digits, get_grouping (category, loc));
}

bool
__grouping_iterator_next (struct grouping_iterator *it)
{
  assert (it->remaining > 0);
  --it->remaining;

  if (it->remaining_in_current_group > 0)
    {
      --it->remaining_in_current_group;
      return false;
    }

  /* If we are in the non-repeating part, switch group.  */
  if (it->remaining < it->non_repeating_groups)
    --it->groupings;

  it->remaining_in_current_group = *it->groupings - 1;
  return true;
}
