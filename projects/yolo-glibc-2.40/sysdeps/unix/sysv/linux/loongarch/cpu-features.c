/* Initialize CPU feature data.  LoongArch64 version.
   This file is part of the GNU C Library.
   Copyright (C) 2024 Free Software Foundation, Inc.

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
   <http://www.gnu.org/licenses/>.  */

#include <cpu-features.h>
#include <elf/dl-hwcaps.h>
#include <elf/dl-tunables.h>
extern void TUNABLE_CALLBACK (set_hwcaps) (tunable_val_t *) attribute_hidden;

static inline void
init_cpu_features (struct cpu_features *cpu_features)
{
  GLRO(dl_larch_cpu_features).hwcap = GLRO(dl_hwcap);
  TUNABLE_GET (glibc, cpu, hwcaps, tunable_val_t *,
	       TUNABLE_CALLBACK (set_hwcaps));
}
