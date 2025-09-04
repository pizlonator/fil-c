/* Audit module loaded by tst-audit23.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>

unsigned int
la_version (unsigned int version)
{
  return LAV_CURRENT;
}

struct map_desc_t
{
  char *lname;
  uintptr_t laddr;
  Lmid_t lmid;
};

void
la_activity (uintptr_t *cookie, unsigned int flag)
{
  fprintf (stderr, "%s: %d %"PRIxPTR"\n", __func__, flag, (uintptr_t) cookie);
}

unsigned int
la_objopen (struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
  const char *l_name = map->l_name[0] == '\0' ? "mainapp" : map->l_name;
  fprintf (stderr, "%s: %"PRIxPTR" %s %"PRIxPTR" %ld\n", __func__,
	   (uintptr_t) cookie, l_name, map->l_addr, lmid);

  struct map_desc_t *map_desc = malloc (sizeof (struct map_desc_t));
  if (map_desc == NULL)
    abort ();

  map_desc->lname = strdup (l_name);
  map_desc->laddr = map->l_addr;
  map_desc->lmid = lmid;

  *cookie = (uintptr_t) map_desc;

  return 0;
}

unsigned int
la_objclose (uintptr_t *cookie)
{
  struct map_desc_t *map_desc = (struct map_desc_t *) *cookie;
  fprintf (stderr, "%s: %"PRIxPTR" %s %"PRIxPTR" %ld\n", __func__,
	   (uintptr_t) cookie, map_desc->lname, map_desc->laddr,
	   map_desc->lmid);

  return 0;
}
