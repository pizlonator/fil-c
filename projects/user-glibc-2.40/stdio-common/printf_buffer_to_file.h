/* Multibyte and wide printf buffers writing data to a FILE * stream.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef PRINTF_BUFFER_TO_FILE_H
#define PRINTF_BUFFER_TO_FILE_H

#include <bits/types/FILE.h>
#include <printf_buffer.h>

struct __printf_buffer_to_file
{
  struct __printf_buffer base;
  FILE *fp;

  /* Staging buffer.  Used if fp does not have any available buffer
     space.  */
  char stage[PRINTF_BUFFER_SIZE_TO_FILE_STAGE];
};

/* Initializes *BUF to write data to FP.  */
void __printf_buffer_to_file_init (struct __printf_buffer_to_file *buf,
                                   FILE *fp) attribute_hidden;

/* Transfers any pending data in BUF to BUF->FP.  The return value
   follows the printf convention (number bytes written; or -1 for error).  */
int __printf_buffer_to_file_done (struct __printf_buffer_to_file *buf)
  attribute_hidden;

/* Wide version below.  */

struct __wprintf_buffer_to_file
{
  struct __wprintf_buffer base;
  FILE *fp;
  wchar_t stage[PRINTF_BUFFER_SIZE_TO_FILE_STAGE];
};
void __wprintf_buffer_to_file_init (struct __wprintf_buffer_to_file *buf,
                                    FILE *fp) attribute_hidden;
int __wprintf_buffer_to_file_done (struct __wprintf_buffer_to_file *buf)
  attribute_hidden;

#endif /* PRINTF_BUFFER_TO_FILE_H */
