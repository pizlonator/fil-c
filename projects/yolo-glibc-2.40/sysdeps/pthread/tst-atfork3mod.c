/* Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <support/check.h>

static void
mod_prepare (void)
{
}

static void
mod_parent (void)
{
}

static void
mod_child (void)
{
}

int atfork3mod_func (void)
{
  TEST_COMPARE (pthread_atfork (mod_prepare, mod_parent, mod_child), 0);

  return 0;
}
