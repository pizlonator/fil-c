/* Test feature wrapper for formatted character input.
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

#include <stdbool.h>
#include <string.h>

#include <support/next_to_fault.h>

/* Reference data is a sequence of characters to match against
   byte-wise.

   For the 's' conversion specifier the number of characters read
   from input includes any leading white-space characters consumed
   as well, so we also consider it a successful match when the ':'
   character following the reference data matches a terminating null
   character in the output produced by the 'scanf' family function
   under test while the character count hasn't been exhausted yet.

   The buffer is preinitialized to contain repeating '\xa5' character
   so as to catch missing data output.  Also no data is expected to be
   written beyond the character sequence received, which is verified
   by checking the following character in the buffer to remain '\xa5'.  */

#define SCANF_BUFFER_SIZE 65536

static struct support_next_to_fault ntf;

#define PREPARE initialize_value_init
static void
initialize_value_init (int argc, char **argv)
{
  ntf = support_next_to_fault_allocate (SCANF_BUFFER_SIZE);
}

static void __attribute__ ((destructor))
initialize_value_fini (void)
{
  support_next_to_fault_free (&ntf);
}

#define pointer_to_value(val) (val)

#define initialize_value(val)						\
do									\
  {									\
    val = ntf.buffer;							\
    memset (val, 0xa5, SCANF_BUFFER_SIZE);				\
  }									\
while (0)

static bool
verify_input (char f, type_t val, long long count, int *errp)
{
  bool match = true;
  int err = 0;
  size_t i;
  int ch;

  for (i = 0; i < count; i++)
    {
      ch = read_input ();
      if (ch < 0)
	{
	  err = ch;
	  goto out;
	}
      if (ch == ':' && val[i] == '\0' && f == 's')
	goto skip;
      if (ch != val[i])
	{
	  match = false;
	  goto out;
	}
    }
  ch = read_input ();
  if (ch < 0)
    {
      err = ch;
      goto out;
    }

skip:
  if (f != 'c' && val[i++] != '\0')
    {
      err = OUTPUT_TERM;
      goto out;
    }
  if (val[i] != '\xa5')
    {
      err = OUTPUT_OVERRUN;
      goto out;
    }

  while (ch != ':')
    {
      ch = read_input ();
      if (ch < 0)
	{
	  err = ch;
	  goto out;
	}
      match = false;
    }

out:
  if (err || !match)
    {
      printf ("error: %s:%d: input buffer: `", __FILE__, __LINE__);
      for (size_t j = 0; j <= i; j++)
	printf ("%c", val[j]);
      printf ("'\n");
    }

  *errp = err;
  return match;
}
