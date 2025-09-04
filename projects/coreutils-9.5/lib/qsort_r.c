/* Reentrant sort function.

   Copyright 2014-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert.  */

#include <config.h>

#include <stdlib.h>

/* This file is compiled only when the system has a qsort_r that needs
   to be replaced because it has the BSD signature rather than the GNU
   signature.  */

struct thunk
{
  int (*cmp) (void const *, void const *, void *);
  void *arg;
};

static int
thunk_cmp (void *thunk, void const *a, void const *b)
{
  struct thunk *th = thunk;
  return th->cmp (a, b, th->arg);
}

void
qsort_r (void *base, size_t nmemb, size_t size,
         int (*cmp) (void const *, void const *, void *),
         void *arg)
{
# undef qsort_r
  struct thunk thunk;
  thunk.cmp = cmp;
  thunk.arg = arg;
  qsort_r (base, nmemb, size, &thunk, thunk_cmp);
}
