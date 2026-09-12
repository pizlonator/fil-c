/* Test ELF symbol version hash collisions: shared object.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

int
foo1 (void)
{
  return 1;
}

int
foo2 (void)
{
  return 2;
}

int
foo3 (void)
{
  return 3;
}

int
foo4 (void)
{
  return 4;
}

int
foo5 (void)
{
  return 5;
}

int
bar1 (void)
{
  return 6;
}

int
bar2 (void)
{
  return 7;
}

int
bar3 (void)
{
  return 8;
}

int
bar4 (void)
{
  return 9;
}

int
bar5 (void)
{
  return 10;
}

symbol_version (foo1, foo, Hxxxyoa);
symbol_version (foo2, foo, HxxxypQ);
symbol_version (foo3, foo, HxxxyqA);
symbol_version (foo4, foo, HxxxzaA);
symbol_version (foo5, foo, Hxxxz_a);
symbol_version (bar1, bar, Hxxxyob);
symbol_version (bar2, bar, HxxxypR);
symbol_version (bar3, bar, HxxxyqB);
symbol_version (bar4, bar, HxxxzaB);
symbol_version (bar5, bar, Hxxxz_b);
