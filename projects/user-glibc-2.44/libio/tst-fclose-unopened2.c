/* Test using fclose on an unopened file.
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

#include <mcheck.h>
#include <stdio.h>
#include <support/check.h>

/* Verify that fclose on an unopened file returns EOF.  This test uses
   a file with an allocated buffer.

   This is not part of the fclose external contract but there are
   dependencies on this behaviour.  */

static int
do_test (void)
{
  mtrace ();

  /* Input file tst-fclose-unopened2.input has 6 bytes plus newline.  */
  char buf[6];

  /* Read from the file to ensure its internal buffer is allocated.  */
  TEST_COMPARE (fread (buf, 1, sizeof (buf), stdin), sizeof (buf));

  TEST_COMPARE (fclose (stdin), 0);

  /* Attempt to close the unopened file and verify that EOF is returned.
     Calling fclose on a file twice normally causes a use-after-free bug,
     however the standard streams are an exception since they are not
     deallocated by fclose.  */
  TEST_COMPARE (fclose (stdin), EOF);

  return 0;
}

#include <support/test-driver.c>
