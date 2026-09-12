/* Test ELF hash collisions: shared object 2.
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

#include <support/check.h>

/* Names with hash collisions for classic ELF hash.  */

int
HxxxynpHxxxynpHxxxynpfoo (void)
{
  return 4;
}

int
HxxxynpHxxxynpHxxxynpHxxxynpfoo (void)
{
  return 5;
}


/* Names with hash collisions for GNU hash.  */

int
gliinmgliinmgliinmbar (void)
{
  return 7;
}

int
gliinmgliinmgliinmgliinmbar (void)
{
  return 6;
}


/* Names with specific hash values for each hash (see
   tst-hash-collision1.c for details).  */

int
HxxyipP (void)
{
  return 14;
}

int
HxxykNp (void)
{
  return 15;
}

int
HxxxzaA (void)
{
  return 19;
}

int
Hxxxz_a (void)
{
  return 20;
}

int
HxxxzaB (void)
{
  return 24;
}

int
Hxxxz_b (void)
{
  return 25;
}

int
gliePJ (void)
{
  return 29;
}

int
gljCpk (void)
{
  return 30;
}

int
gliePK (void)
{
  return 34;
}

int
gljCpl (void)
{
  return 35;
}

int
gliePL (void)
{
  return 39;
}

int
gljCpm (void)
{
  return 40;
}

int
Adfn9ru (void)
{
  return 44;
}

int
Adfn9sT (void)
{
  return 45;
}

int
Adfn9rv (void)
{
  return 49;
}

int
Adfn9sU (void)
{
  return 50;
}

int
Adfn9rw (void)
{
  return 54;
}

int
Adfn9sV (void)
{
  return 55;
}

int
Adfn9rx (void)
{
  return 59;
}

int
Adfn9sW (void)
{
  return 60;
}

int
gliePH (void)
{
  return 64;
}

int
gljCpi (void)
{
  return 65;
}

int
gliePI (void)
{
  return 69;
}

int
gljCpj (void)
{
  return 70;
}
