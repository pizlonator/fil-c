/* gtkpopcountprivate.h: Private implementation of popcount
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if (defined(_MSC_VER) && !_M_ARM && !_M_ARM64) && (__AVX__ || __SSE4_2__ || __POPCNT__)
#include <intrin.h>

static inline guint
gtk_popcount (guint32 value)
{
  return __popcnt (value);
}
#elif defined(__GNUC__) || defined(__clang__)
#define gtk_popcount(v) __builtin_popcount (v)
#else
static inline guint
gtk_popcount (guint32 value)
{
  /* http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel */
  return (((value & 0xfff) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f) +
         ((((value & 0xfff000) >> 12) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f) +
         (((value >> 24) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f);
}
#endif
