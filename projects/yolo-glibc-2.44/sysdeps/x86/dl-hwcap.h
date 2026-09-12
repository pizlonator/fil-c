/* x86 version of hardware capability information handling macros.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.

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

#ifndef _DL_HWCAP_H
#define _DL_HWCAP_H

enum
{
  HWCAP_X86_SSE2		= 1 << 0,
  HWCAP_X86_64			= 1 << 1,
  HWCAP_X86_AVX512_1		= 1 << 2
};

#endif /* dl-hwcap.h */
