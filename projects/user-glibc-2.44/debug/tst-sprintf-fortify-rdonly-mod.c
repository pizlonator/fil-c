/* Testcase for BZ 30932.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *str2 = "F";
static char writeable_format[10] = "%s";
static char relro_format[10] __attribute__ ((section (".data.rel.ro"))) =
  "%s%n%s%n";

void
init_writable (void)
{
  strcpy (writeable_format + 2, "%n%s%n");
}

int
sprintf_writable (int *n1, int *n2)
{
  char buf[128];
  return sprintf (buf, writeable_format, str2, n1, str2, n2);
}

int
sprintf_relro (int *n1, int *n2)
{
  char buf[128];
  return sprintf (buf, relro_format, str2, n1, str2, n2);
}

int
sprintf_writable_malloc (int *n1, int *n2)
{
  char buf[128];
  char *buf2_malloc = strdup (writeable_format);
  if (buf2_malloc == NULL)
    abort ();
  return sprintf (buf, buf2_malloc, str2, n1, str2, n2);
}
