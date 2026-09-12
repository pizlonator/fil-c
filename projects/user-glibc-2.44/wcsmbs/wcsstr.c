/* Locate a substring in a wide-character string.
   Copyright (C) 1995-2026 Free Software Foundation, Inc.
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

#include <wchar.h>
#include <string.h>

#define AVAILABLE(h, h_l, j, n_l)					\
  (((j) + (n_l) <= (h_l))						\
   || ((h_l) += __wcsnlen ((void*)((h) + (h_l)), (n_l) + 128),		\
       (j) + (n_l) <= (h_l)))
#include "wcs-two-way.h"

#ifndef WCSSTR
# define WCSSTR wcsstr
#endif

wchar_t *
WCSSTR (const wchar_t *haystack, const wchar_t *needle)
{
  /* Ensure haystack length is at least as long as needle length.
     Since a match may occur early on in a huge haystack, use strnlen
     and read ahead a few cachelines for improved performance.  */
  size_t ne_len = __wcslen (needle);
  size_t hs_len = __wcsnlen (haystack, ne_len | 128);
  if (hs_len < ne_len)
    return NULL;

  /* Check whether we have a match.  This improves performance since we
     avoid initialization overheads.  */
  if (__wmemcmp (haystack, needle, ne_len) == 0)
    return (wchar_t *) haystack;

  return two_way_short_needle (haystack, hs_len, needle, ne_len);
}
/* This alias is for backward compatibility with drafts of the ISO C
   standard.  Unfortunately the Unix(TM) standard requires this name.  */
weak_alias (wcsstr, wcswcs)
