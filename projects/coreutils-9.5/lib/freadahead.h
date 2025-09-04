/* Retrieve information about a FILE stream.
   Copyright (C) 2007-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* This file uses _GL_ATTRIBUTE_PURE, HAVE___FREADAHEAD.  */
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>
#include <stdio.h>

/* Assuming the stream STREAM is open for reading:
   Return the number of bytes waiting in the input buffer of STREAM.
   This includes both the bytes that have been read from the underlying input
   source and the bytes that have been pushed back through 'ungetc'.

   If this number is 0 and the stream is not currently writing,
   fflush (STREAM) is known to be a no-op.

   STREAM must not be wide-character oriented.  */

#if HAVE___FREADAHEAD /* musl libc, Android API level ≥ 33 */

# include <stdio_ext.h>
# define freadahead(stream) __freadahead (stream)

#else

# ifdef __cplusplus
extern "C" {
# endif

extern size_t freadahead (FILE *stream) _GL_ATTRIBUTE_PURE;

# ifdef __cplusplus
}
# endif

#endif
