/* HWCAP string information.  PowerPC version.
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

#ifndef _DL_HWCAP_INFO_H
#define _DL_HWCAP_INFO_H

#include <stddef.h>
#include <sys/auxv.h>

extern const char __dl_hwcap_names[] attribute_hidden;

struct hwcap_info_t
{
  unsigned int value;
  int hwcap;
};
extern const struct hwcap_info_t __dl_hwcap_info[] attribute_hidden;
extern size_t __dl_hwcap_info_size attribute_hidden;

#endif
