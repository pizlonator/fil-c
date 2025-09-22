/* Return scope DIEs containing given DIE.
   Copyright (C) 2005, 2015 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "libdwP.h"
#include <assert.h>
#include <stdlib.h>

static int
scope_visitor (unsigned int depth, struct Dwarf_Die_Chain *die, void *arg)
{
  if (die->die.addr != *(void **) arg)
    return 0;

  Dwarf_Die *scopes = malloc (depth * sizeof scopes[0]);
  if (scopes == NULL)
    {
      __libdw_seterrno (DWARF_E_NOMEM);
      return -1;
    }

  unsigned int i = 0;
  do
    {
      scopes[i++] = die->die;
      die = die->parent;
    }
  while (die != NULL);
  assert (i == depth);

  *(void **) arg = scopes;
  return depth;
}

int
dwarf_getscopes_die (Dwarf_Die *die, Dwarf_Die **scopes)
{
  if (die == NULL)
    return -1;

  struct Dwarf_Die_Chain cu = { .die = CUDIE (die->cu), .parent = NULL };
  void *info = die->addr;
  int result = __libdw_visit_scopes (1, &cu, NULL, &scope_visitor, NULL, &info);
  if (result > 0)
    *scopes = info;
  return result;
}
