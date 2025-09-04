/* Test of verror.h functions.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.

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

/* Written by Bruno Haible <bruno@clisp.org>, 2023.  */

#include <config.h>

#include "verror.h"

#include <errno.h>
#include <stdarg.h>

#include <error.h>
#include "macros.h"

/* Custom function to not show the program name in error messages.  */
static void
print_no_progname (void)
{
}

static void
test_zero (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  verror (0, 0, format, args);
  va_end (args);
}

static void
test_zero_at_line (const char *filename, unsigned int lineno,
                   const char *format, ...)
{
  va_list args;

  va_start (args, format);
  verror_at_line (0, 0, filename, lineno, format, args);
  va_end (args);
}

static void
test_errnum (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  verror (0, EACCES, format, args);
  va_end (args);
}

static void
test_fatal (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  verror (4, 0, format, args);
  va_end (args);
}

int
main (int argc, char *argv[])
{
  /* Test verror() function with zero STATUS and zero ERRNUM.  */
  test_zero ("bummer");
  /* With format string arguments.  */
  errno = EINVAL; /* should be ignored */
  test_zero ("Zonk %d%d%d is too large", 1, 2, 3);
  /* With non-ASCII characters.  */
  test_zero ("Pokémon started");
  /* Verify error_message_count.  */
  ASSERT (error_message_count == 3);

  /* Test verror_at_line() function with zero STATUS and zero ERRNUM.  */
  test_zero_at_line ("d1/foo.c", 10, "invalid blub");
  test_zero_at_line ("d1/foo.c", 10, "invalid blarn");
  /* Verify error_message_count.  */
  ASSERT (error_message_count == 5);

  /* Test error_one_per_line.  */
  error_one_per_line = 1;
  test_zero_at_line ("d1/foo.c", 10, "unsupported glink");
  /* Another line number.  */
  test_zero_at_line ("d1/foo.c", 13, "invalid brump");
  /* Another file name.  */
  test_zero_at_line ("d2/foo.c", 13, "unsupported flinge");
  /* Same file name and same line number => message not shown.  */
  test_zero_at_line ("d2/foo.c", 13, "invalid bark");
  /* Verify error_message_count.  */
  ASSERT (error_message_count == 8);
  error_one_per_line = 0;

  /* Test error_print_progname.  */
  error_print_progname = print_no_progname;
  test_zero ("hammer");
  test_zero ("boing %d%d%d is too large", 1, 2, 3);
  #if 0
  /* The documentation does not describe the output if the file name is NULL. */
  test_zero_at_line (NULL, 42, "drummer too loud");
  #endif
  test_zero_at_line ("d2/bar.c", 11, "bark too loud");
  /* Verify error_message_count.  */
  ASSERT (error_message_count == 11);
  error_print_progname = NULL;

  /* Test verror() function with nonzero ERRNUM.  */
  errno = EINVAL; /* should be ignored */
  test_errnum ("can't steal");
  /* Verify error_message_count.  */
  ASSERT (error_message_count == 12);

  /* Test verror() function with nonzero STATUS.  */
  test_fatal ("fatal error");

  return 0;
}
