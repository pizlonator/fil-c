/* Print CPU diagnostics data in ld.so.  PowerPC version.
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
#include <dl-hwcap-info.h>
#include <ldsodefs.h>
#include <sys/auxv.h>

static void
print_hwcap_value (const char *label, int hwcap, unsigned long int value)
{
  _dl_printf ("powerpc.cpu_features.%s=\"", label);
  size_t offset = 0;
  for (int i = 0; i < __dl_hwcap_info_size; ++i)
    {
      const char *hwcap_name = __dl_hwcap_names + offset;
      size_t hwcap_name_len = strlen (hwcap_name);
      if (hwcap == __dl_hwcap_info[i].hwcap
	  && value & __dl_hwcap_info[i].value)
	_dl_printf ("%s ", hwcap_name);
      offset += hwcap_name_len + 1;
    }
  _dl_printf ("\"\n");
}

static void
print_cache_geometry_value (const char *label, unsigned long int geometry)
{
  unsigned long int assocty, line;

  _dl_printf ("powerpc.cpu_features.%s=\"", label);

  line = geometry & 0xffff;
  assocty = (geometry >> 16) & 0xffff;

  if (line == 0)
    _dl_printf ("Unknown line size, ");
  else
    _dl_printf ("%luB line size, ", line);

  switch (assocty)
    {
    case 0:
      _dl_printf ("Unknown associativity");
      break;
    case 1:
      _dl_printf ("Directly mapped");
      break;
    case 0xffff:
      _dl_printf ("Fully associative");
      break;
    default:
      _dl_printf ("%lu-way set associative", assocty);
    }
  _dl_printf ("\"\n");
}

void
_dl_diagnostics_cpu (void)
{
  print_hwcap_value ("hwcap", AT_HWCAP, GLRO(dl_hwcap));
  print_hwcap_value ("hwcap2", AT_HWCAP2, GLRO(dl_hwcap2));
  print_hwcap_value ("hwcap3", AT_HWCAP3, GLRO(dl_hwcap3));
  print_hwcap_value ("hwcap4", AT_HWCAP4, GLRO(dl_hwcap4));

  for (ElfW(auxv_t) *av = GLRO(dl_auxv); av->a_type != AT_NULL; ++av)
    switch (av->a_type)
      {
      case AT_L1I_CACHEGEOMETRY:
        print_cache_geometry_value ("l1i_cachegeometry", av->a_un.a_val);
        break;
      case AT_L1D_CACHEGEOMETRY:
        print_cache_geometry_value ("l1d_cachegeometry", av->a_un.a_val);
        break;
      case AT_L2_CACHEGEOMETRY:
        print_cache_geometry_value ("l2_cachegeometry", av->a_un.a_val);
        break;
      case AT_L3_CACHEGEOMETRY:
        print_cache_geometry_value ("l3_cachegeometry", av->a_un.a_val);
        break;
      }
}
