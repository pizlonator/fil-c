/* Test ELF hash collisions.
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
extern int foo (void);
extern int Hxxxynpfoo (void);
extern int HxxxynpHxxxynpfoo (void);
extern int HxxxynpHxxxynpHxxxynpfoo (void);
extern int HxxxynpHxxxynpHxxxynpHxxxynpfoo (void);

/* Names with hash collisions for GNU hash.  */
extern int bar (void);
extern int gliinmbar (void);
extern int gliinmgliinmbar (void);
extern int gliinmgliinmgliinmbar (void);
extern int gliinmgliinmgliinmgliinmbar (void);

/* Classic ELF hash 0.  */
extern int Hxxxynp (void);
extern int HxxxypP (void);
extern int Hxxyinp (void);
extern int HxxyipP (void);
extern int HxxykNp (void);

/* Classic ELF hash 1.  */
extern int Hxxxyoa (void);
extern int HxxxypQ (void);
extern int HxxxyqA (void);
extern int HxxxzaA (void);
extern int Hxxxz_a (void);

/* Classic ELF hash 2.  */
extern int Hxxxyob (void);
extern int HxxxypR (void);
extern int HxxxyqB (void);
extern int HxxxzaB (void);
extern int Hxxxz_b (void);

/* GNU hash 0.  */
extern int glidpk (void);
extern int glidqJ (void);
extern int glieOk (void);
extern int gliePJ (void);
extern int gljCpk (void);

/* GNU hash 1.  */
extern int glidpl (void);
extern int glidqK (void);
extern int glieOl (void);
extern int gliePK (void);
extern int gljCpl (void);

/* GNU hash 2.  */
extern int glidpm (void);
extern int glidqL (void);
extern int glieOm (void);
extern int gliePL (void);
extern int gljCpm (void);

/* GNU hash 0x7ffffffe.  */
extern int AdfmZru (void);
extern int AdfmZsT (void);
extern int AdfmZt3 (void);
extern int Adfn9ru (void);
extern int Adfn9sT (void);

/* GNU hash 0x7fffffff.  */
extern int AdfmZrv (void);
extern int AdfmZsU (void);
extern int AdfmZt4 (void);
extern int Adfn9rv (void);
extern int Adfn9sU (void);

/* GNU hash 0x80000000.  */
extern int AdfmZrw (void);
extern int AdfmZsV (void);
extern int AdfmZt5 (void);
extern int Adfn9rw (void);
extern int Adfn9sV (void);

/* GNU hash 0x80000001.  */
extern int AdfmZrx (void);
extern int AdfmZsW (void);
extern int AdfmZt6 (void);
extern int Adfn9rx (void);
extern int Adfn9sW (void);

/* GNU hash 0xfffffffe.  */
extern int glidpi (void);
extern int glidqH (void);
extern int glieOi (void);
extern int gliePH (void);
extern int gljCpi (void);

/* GNU hash 0xffffffff.  */
extern int glidpj (void);
extern int glidqI (void);
extern int glieOj (void);
extern int gliePI (void);
extern int gljCpj (void);


int
do_test (void)
{
  TEST_COMPARE (foo (), 1);
  TEST_COMPARE (Hxxxynpfoo (), 2);
  TEST_COMPARE (HxxxynpHxxxynpfoo (), 3);
  TEST_COMPARE (HxxxynpHxxxynpHxxxynpfoo (), 4);
  TEST_COMPARE (HxxxynpHxxxynpHxxxynpHxxxynpfoo (), 5);
  TEST_COMPARE (gliinmgliinmgliinmgliinmbar (), 6);
  TEST_COMPARE (gliinmgliinmgliinmbar (), 7);
  TEST_COMPARE (gliinmgliinmbar (), 8);
  TEST_COMPARE (gliinmbar (), 9);
  TEST_COMPARE (bar (), 10);
  TEST_COMPARE (Hxxxynp (), 11);
  TEST_COMPARE (HxxxypP (), 12);
  TEST_COMPARE (Hxxyinp (), 13);
  TEST_COMPARE (HxxyipP (), 14);
  TEST_COMPARE (HxxykNp (), 15);
  TEST_COMPARE (Hxxxyoa (), 16);
  TEST_COMPARE (HxxxypQ (), 17);
  TEST_COMPARE (HxxxyqA (), 18);
  TEST_COMPARE (HxxxzaA (), 19);
  TEST_COMPARE (Hxxxz_a (), 20);
  TEST_COMPARE (Hxxxyob (), 21);
  TEST_COMPARE (HxxxypR (), 22);
  TEST_COMPARE (HxxxyqB (), 23);
  TEST_COMPARE (HxxxzaB (), 24);
  TEST_COMPARE (Hxxxz_b (), 25);
  TEST_COMPARE (glidpk (), 26);
  TEST_COMPARE (glidqJ (), 27);
  TEST_COMPARE (glieOk (), 28);
  TEST_COMPARE (gliePJ (), 29);
  TEST_COMPARE (gljCpk (), 30);
  TEST_COMPARE (glidpl (), 31);
  TEST_COMPARE (glidqK (), 32);
  TEST_COMPARE (glieOl (), 33);
  TEST_COMPARE (gliePK (), 34);
  TEST_COMPARE (gljCpl (), 35);
  TEST_COMPARE (glidpm (), 36);
  TEST_COMPARE (glidqL (), 37);
  TEST_COMPARE (glieOm (), 38);
  TEST_COMPARE (gliePL (), 39);
  TEST_COMPARE (gljCpm (), 40);
  TEST_COMPARE (AdfmZru (), 41);
  TEST_COMPARE (AdfmZsT (), 42);
  TEST_COMPARE (AdfmZt3 (), 43);
  TEST_COMPARE (Adfn9ru (), 44);
  TEST_COMPARE (Adfn9sT (), 45);
  TEST_COMPARE (AdfmZrv (), 46);
  TEST_COMPARE (AdfmZsU (), 47);
  TEST_COMPARE (AdfmZt4 (), 48);
  TEST_COMPARE (Adfn9rv (), 49);
  TEST_COMPARE (Adfn9sU (), 50);
  TEST_COMPARE (AdfmZrw (), 51);
  TEST_COMPARE (AdfmZsV (), 52);
  TEST_COMPARE (AdfmZt5 (), 53);
  TEST_COMPARE (Adfn9rw (), 54);
  TEST_COMPARE (Adfn9sV (), 55);
  TEST_COMPARE (AdfmZrx (), 56);
  TEST_COMPARE (AdfmZsW (), 57);
  TEST_COMPARE (AdfmZt6 (), 58);
  TEST_COMPARE (Adfn9rx (), 59);
  TEST_COMPARE (Adfn9sW (), 60);
  TEST_COMPARE (glidpi (), 61);
  TEST_COMPARE (glidqH (), 62);
  TEST_COMPARE (glieOi (), 63);
  TEST_COMPARE (gliePH (), 64);
  TEST_COMPARE (gljCpi (), 65);
  TEST_COMPARE (glidpj (), 66);
  TEST_COMPARE (glidqI (), 67);
  TEST_COMPARE (glieOj (), 68);
  TEST_COMPARE (gliePI (), 69);
  TEST_COMPARE (gljCpj (), 70);
  return 0;
}

#include <support/test-driver.c>
