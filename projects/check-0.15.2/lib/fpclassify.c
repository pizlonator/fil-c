/* Copyright (C) 2017, bel2125
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
/* Note: Within "check", this file uses the LGPL license.
 * If you need only this file, you may find a MIT licensed version in
 * the "floatmagic" repository of the initial author.
 */

#include "libcompat.h"

double DOUBLE_ZERO = 0.0;

#if defined(NEED_fpclassify)

#if defined(HAVE_STDINT_H)
#include <stdint.h>
typedef uint64_t bitfield64;
#elif defined(_MSC_VER)
typedef unsigned __int64 bitfield64;
#else
typedef unsigned long long bitfield64;
#endif

static bitfield64 ms = 0x8000000000000000;
static bitfield64 me = 0x7FF0000000000000;
static bitfield64 mf = 0x000FFFFFFFFFFFFF;

int fpclassify(double d) 
{
  bitfield64 *p = (bitfield64 *)&d;
  if ((*p & me) != me) {
    /* finite */
    if (*p & mf) {
      /* finite and not null */
      if (*p & me) {
        return FP_NORMAL;
      }
      return FP_SUBNORMAL;
    }	
    return FP_ZERO;
  }
  if (*p & mf) {
    return FP_NAN;
  }
  return FP_INFINITE;
}

#endif

