/* Test that program loading does not call malloc.
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


#include <string.h>
#include <unistd.h>

static void
print (const char *s)
{
  const char *end = s + strlen (s);
  while (s < end)
    {
      ssize_t ret = write (STDOUT_FILENO, s, end - s);
      if (ret <= 0)
        _exit (2);
      s += ret;
    }
}

static void __attribute__ ((noreturn))
unexpected_call (const char *function)
{
  print ("error: unexpected call to ");
  print (function);
  print ("\n");
  _exit (1);
}

/* These are the malloc functions implement in elf/dl-minimal.c.  */

void
free (void *ignored)
{
  unexpected_call ("free");
}

void *
calloc (size_t ignored1, size_t ignored2)
{
  unexpected_call ("calloc");
}

void *
malloc (size_t ignored)
{
  unexpected_call ("malloc");
}

void *
realloc (void *ignored1, size_t ignored2)
{
  unexpected_call ("realloc");
}

int
main (void)
{
  /* Do not use the test wrapper, to avoid spurious malloc calls from it.  */
  return 0;
}
