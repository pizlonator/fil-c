/* Test ELF hash collisions: shared object 1.
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
foo (void)
{
  return 1;
}

int
Hxxxynpfoo (void)
{
  return 2;
}

int
HxxxynpHxxxynpfoo (void)
{
  return 3;
}


/* Names with hash collisions for GNU hash.  */

int
bar (void)
{
  return 10;
}

int
gliinmbar (void)
{
  return 9;
}

int
gliinmgliinmbar (void)
{
  return 8;
}


/* Names with specific hash values for each hash (see
   tst-hash-collision1.c for details).  */

int
Hxxxynp (void)
{
  return 11;
}

int
HxxxypP (void)
{
  return 12;
}

int
Hxxyinp (void)
{
  return 13;
}

int
Hxxxyoa (void)
{
  return 16;
}

int
HxxxypQ (void)
{
  return 17;
}

int
HxxxyqA (void)
{
  return 18;
}

int
Hxxxyob (void)
{
  return 21;
}

int
HxxxypR (void)
{
  return 22;
}

int
HxxxyqB (void)
{
  return 23;
}

int
glidpk (void)
{
  return 26;
}

int
glidqJ (void)
{
  return 27;
}

int
glieOk (void)
{
  return 28;
}

int
glidpl (void)
{
  return 31;
}

int
glidqK (void)
{
  return 32;
}

int
glieOl (void)
{
  return 33;
}

int
glidpm (void)
{
  return 36;
}

int
glidqL (void)
{
  return 37;
}

int
glieOm (void)
{
  return 38;
}

int
AdfmZru (void)
{
  return 41;
}

int
AdfmZsT (void)
{
  return 42;
}

int
AdfmZt3 (void)
{
  return 43;
}

int
AdfmZrv (void)
{
  return 46;
}

int
AdfmZsU (void)
{
  return 47;
}

int
AdfmZt4 (void)
{
  return 48;
}

int
AdfmZrw (void)
{
  return 51;
}

int
AdfmZsV (void)
{
  return 52;
}

int
AdfmZt5 (void)
{
  return 53;
}

int
AdfmZrx (void)
{
  return 56;
}

int
AdfmZsW (void)
{
  return 57;
}

int
AdfmZt6 (void)
{
  return 58;
}

int
glidpi (void)
{
  return 61;
}

int
glidqH (void)
{
  return 62;
}

int
glieOi (void)
{
  return 63;
}

int
glidpj (void)
{
  return 66;
}

int
glidqI (void)
{
  return 67;
}

int
glieOj (void)
{
  return 68;
}
