/* Print CPU diagnostics data in ld.so.  AArch64 version.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <cpu-features.h>
#include <dl-iterate_cpu.h>
#include <ldsodefs.h>
#include <sys/auxv.h>

static void
print_cpu_features_value (const char *label, uint64_t value)
{
  _dl_printf ("aarch64.cpu_features.");
  _dl_diagnostics_print_labeled_value (label, value);
}

static void
print_per_cpu_value (const struct dl_iterate_cpu *dic,
                     const char *label, uint64_t value)
{
  _dl_printf ("aarch64.processor[0x%x].", dic->processor_index);
  _dl_diagnostics_print_labeled_value (label, value);
}

void
_dl_diagnostics_cpu (void)
{
  print_cpu_features_value ("bti", GLRO (dl_aarch64_cpu_features).bti);
  print_cpu_features_value ("midr_el1",
                            GLRO (dl_aarch64_cpu_features).midr_el1);
  print_cpu_features_value ("mops", GLRO (dl_aarch64_cpu_features).mops);
  print_cpu_features_value ("mte_state",
                            GLRO (dl_aarch64_cpu_features).mte_state);
  print_cpu_features_value ("prefer_sve_ifuncs",
                            GLRO (dl_aarch64_cpu_features).prefer_sve_ifuncs);
  print_cpu_features_value ("sve", GLRO (dl_aarch64_cpu_features).sve);
  print_cpu_features_value ("zva_size",
                            GLRO (dl_aarch64_cpu_features).zva_size);

  struct dl_iterate_cpu dic;
  _dl_iterate_cpu_init (&dic);

  while (_dl_iterate_cpu_next (&dic))
    {
      if (dic.requested_cpu >= 0)
        _dl_printf ("aarch64.processor[0x%x].requested=0x%x\n",
                    dic.processor_index, dic.requested_cpu);
      if (dic.actual_cpu >= 0)
        _dl_printf ("aarch64.processor[0x%x].observed=0x%x\n",
                    dic.processor_index, dic.actual_cpu);
      if (dic.actual_node >= 0)
        _dl_printf ("aarch64.processor[0x%x].observed_node=0x%x\n",
                    dic.processor_index, dic.actual_node);

      if (GLRO (dl_hwcap) & HWCAP_CPUID)
        {
          uint64_t midr_el1;
          asm ("mrs %0, midr_el1" : "=r" (midr_el1));
          print_per_cpu_value (&dic, "midr_el1", midr_el1);
        }

      {
        uint64_t dczid_el0;
        asm ("mrs %0, dczid_el0" : "=r" (dczid_el0));
        print_per_cpu_value (&dic, "dczid_el0", dczid_el0);
      }
    }
}
