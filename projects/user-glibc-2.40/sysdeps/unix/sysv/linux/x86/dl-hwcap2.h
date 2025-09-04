/* Handle AT_HWCAP2.  Linux/x86 version.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.

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

/* The following must match the kernel's <asm/hwcap2.h>.  */
#define HWCAP2_RING3MWAIT	(1 << 0)
#define HWCAP2_FSGSBASE		(1 << 1)

static inline void
dl_check_hwcap2 (struct cpu_features *cpu_features)
{
  if ((GLRO(dl_hwcap2) & HWCAP2_FSGSBASE) != 0)
    CPU_FEATURE_SET_ACTIVE (cpu_features, FSGSBASE);
}
