/* Test of getting the amount of total/available physical memory.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2023.  */

#include <config.h>

#include "physmem.h"

#include <stdio.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  printf ("Total memory:     %12.f B = %6.f MiB\n",
          physmem_total (), physmem_total () / (1024.0 * 1024.0));
  printf ("Available memory: %12.f B = %6.f MiB\n",
          physmem_available (), physmem_available () / (1024.0 * 1024.0));
  ASSERT (physmem_total () >= physmem_available ());
  ASSERT (physmem_available () >= 4 * 1024 * 1024);

  return 0;
}
