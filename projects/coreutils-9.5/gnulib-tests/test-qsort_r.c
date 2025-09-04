/* Test qsort_r.

   Copyright 2014-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert.  */

#include <config.h>

#include <stdlib.h>
#include <string.h>

static int
cmp (void const *va, void const *vb, void *varg)
{
  char const *a = va;
  char const *b = vb;
  int *arg = varg;
  return (*a < *b ? -1 : *a > *b) * *arg;
}

int
main (void)
{
  char buf[] = "thequickbrownfoxjumpedoverthelazydogs";
  int forward = 1;
  int reverse = -1;
  qsort_r (buf, sizeof buf - 1, 1, cmp, &forward);
  if (strcmp (buf, "abcddeeeefghhijklmnoooopqrrsttuuvwxyz") != 0)
    return 1;
  qsort_r (buf, sizeof buf - 1, 1, cmp, &reverse);
  if (strcmp (buf, "zyxwvuuttsrrqpoooonmlkjihhgfeeeeddcba") != 0)
    return 1;
  return 0;
}
