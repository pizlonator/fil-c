/* Functionality for checking file contents.
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

#ifndef SUPPORT_FILE_CONTENTS_H
#define SUPPORT_FILE_CONTENTS_H

#include <support/check.h>
#include <stdio.h>

__BEGIN_DECLS

/* Check that an already-open file has exactly the given bytes,
   starting at the current location in the file.  The file position
   indicator is updated to point after the bytes compared.  Return 0
   if equal, 1 otherwise or on read error.  */
int support_compare_file_bytes (FILE *fp, const char *contents, size_t length);

/* Check that an already-open file has exactly the given string as
   contents, starting at the current offset.  The file position
   indicator is updated to point after the bytes compared.  Return 0
   if equal, 1 otherwise or on read error.  */
int support_compare_file_string (FILE *fp, const char *contents);

/* Check that a not-currently-open file has exactly the given bytes.
   Return 0 if equal, 1 otherwise or on read error.  */
int support_open_and_compare_file_bytes (const char *file,
					 const char *contents,
					 size_t length);

/* Check that a not-currently-open file has exactly the given string
   as contents, starting at the current offset.  Return 0 if equal, 1
   otherwise or on read error.  */
int support_open_and_compare_file_string (const char *file,
					  const char *contents);

/* Compare bytes read from an open file with the given string.  The
   file position indicator is updated to point after the bytes
   compared.  */
#define TEST_COMPARE_FILE_STRING(FP, CONTENTS)			\
  TEST_COMPARE (support_compare_file_string (FP, CONTENTS), 0)

/* Read a file and compare bytes read from it with the given string.  */
#define TEST_OPEN_AND_COMPARE_FILE_STRING(FILE, CONTENTS)		\
  TEST_COMPARE (support_open_and_compare_file_string (FILE, CONTENTS), 0)

__END_DECLS

#endif /* SUPPORT_FILE_CONTENTS_H */
