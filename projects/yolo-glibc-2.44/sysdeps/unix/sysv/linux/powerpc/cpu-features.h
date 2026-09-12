/* Initialize cpu feature data.  PowerPC version.
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

#ifndef __CPU_FEATURES_POWERPC_H
# define __CPU_FEATURES_POWERPC_H

#include <stdbool.h>
#include <sys/auxv.h>

struct cpu_features
{
  bool use_cached_memopt;
  unsigned long int hwcap;
  unsigned long int hwcap2;
  unsigned long int hwcap3;
  unsigned long int hwcap4;
};

#endif /* __CPU_FEATURES_H  */
