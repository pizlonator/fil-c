/* Check DTAUDIT and vDSO interaction.
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

#include <link.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/auxv.h>

static inline bool
startswith (const char *str, const char *pre)
{
  size_t lenpre = strlen (pre);
  size_t lenstr = strlen (str);
  return lenstr < lenpre ? false : memcmp (pre, str, lenpre) == 0;
}

unsigned int
la_version (unsigned int version)
{
  return LAV_CURRENT;
}

unsigned int
la_objopen (struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
  /* The linux-gate.so is placed at a fixed address, thus l_addr being 0,
     and it might be the value reported as the AT_SYSINFO_EHDR.  */
  if (map->l_addr == 0 && startswith (map->l_name, "linux-gate.so"))
    fprintf (stderr, "vdso found: %p\n", NULL);
  else if (map->l_addr == getauxval (AT_SYSINFO_EHDR))
    fprintf (stderr, "vdso found: %p\n", (void*) map->l_addr);

  return 0;
}
