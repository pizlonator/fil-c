/* FILE * interface to a struct __*printf_buffer.
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

/* Registered printf format specifier callbacks produce data via a
   FILE * stream.  struct __printf_buffer_as_file enables vfprintf to
   create a suitable stdio stream.  Likewise struct
   __wprintf_buffer_as_file for vfwprintf.  */

#ifndef PRINTF_BUFFER_AS_FILE_H
#define PRINTF_BUFFER_AS_FILE_H

#include <libio/libioP.h>

struct __printf_buffer;

struct __printf_buffer_as_file
{
  /* Interface to libio.  */
  FILE stream;
  const struct _IO_jump_t *vtable;

  /* Pointer to the underlying buffer.  */
  struct __printf_buffer *next;
};

/* Initialization *FP so that data written to its FILE * stream ends
   up in NEXT.  */
void __printf_buffer_as_file_init (struct __printf_buffer_as_file *fp,
                                   struct __printf_buffer *next)
  attribute_hidden;

/* Returns the FILE * that can be used to write data to the
   buffer.  */
static inline FILE *
__printf_buffer_as_file_get (struct __printf_buffer_as_file *file)
{
  return &file->stream;
}

/* Transfers all pending data from the FILE * to the underlying
   buffer.  Returns true if there have been no errors.  */
bool __printf_buffer_as_file_terminate (struct __printf_buffer_as_file *)
  attribute_hidden;

/* Wide variant follows.  */

struct __wprintf_buffer;
struct __wprintf_buffer_as_file
{
  /* Interface to libio.  */
  FILE stream;
  const struct _IO_jump_t *vtable;
  struct _IO_wide_data wide_stream;

  /* Pointer to the underlying buffer.  */
  struct __wprintf_buffer *next;
};

void __wprintf_buffer_as_file_init (struct __wprintf_buffer_as_file *fp,
                                    struct __wprintf_buffer *next)
  attribute_hidden;

static inline FILE *
__wprintf_buffer_as_file_get (struct __wprintf_buffer_as_file *file)
{
  return &file->stream;
}

bool __wprintf_buffer_as_file_terminate (struct __wprintf_buffer_as_file *)
  attribute_hidden;

#endif /* PRINTF_BUFFER_AS_FILE_H */
