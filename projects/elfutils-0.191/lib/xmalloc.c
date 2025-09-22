/* Convenience functions for allocation.
   Copyright (C) 2006, 2015 Red Hat, Inc.
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

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include "system.h"


/* Allocate N bytes of memory dynamically, with error checking.  */
void *
xmalloc (size_t n)
{
  void *p;

  p = malloc (n);
  if (p == NULL)
    error (EXIT_FAILURE, 0, _("memory exhausted"));
  return p;
}


/* Allocate memory for N elements of S bytes, with error checking.  */
void *
xcalloc (size_t n, size_t s)
{
  void *p;

  p = calloc (n, s);
  if (p == NULL)
    error (EXIT_FAILURE, 0, _("memory exhausted"));
  return p;
}


/* Change the size of an allocated block of memory P to N bytes,
   with error checking.  */
void *
xrealloc (void *p, size_t n)
{
  p = realloc (p, n);
  if (p == NULL)
    error (EXIT_FAILURE, 0, _("memory exhausted"));
  return p;
}
