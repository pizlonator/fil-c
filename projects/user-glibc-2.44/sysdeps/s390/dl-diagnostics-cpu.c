/* Print CPU diagnostics data in ld.so.  s390 version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <dl-diagnostics.h>
#include <ldsodefs.h>
#include <cpu-features.h>

static void
print_cpu_features_value (const char *label, uint64_t value)
{
  _dl_printf ("s390.cpu_features.");
  _dl_diagnostics_print_labeled_value (label, value);
}

void
_dl_diagnostics_cpu (void)
{
  const struct cpu_features *cpu_features = &GLRO(dl_s390_cpu_features);
  print_cpu_features_value ("hwcap", cpu_features->hwcap);
  print_cpu_features_value ("stfle_orig", cpu_features->stfle_orig);
  print_cpu_features_value ("stfle_filtered", cpu_features->stfle_filtered);
}
