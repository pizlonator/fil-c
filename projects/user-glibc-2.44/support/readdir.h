/* Type-generic wrapper for readdir functions.
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

#ifndef SUPPORT_READDIR_H
#define SUPPORT_READDIR_H

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

/* Definition independent of _FILE_OFFSET_BITS.  */
struct support_dirent
{
  uint64_t d_ino;
  uint64_t d_off;               /* 0 if d_off is not supported.  */
  uint32_t d_type;
  char *d_name;
};

/* Operation to be performed by support_readdir below.  */
enum support_readdir_op
  {
    SUPPORT_READDIR,
    SUPPORT_READDIR64,
    SUPPORT_READDIR_R,
    SUPPORT_READDIR64_R,
    SUPPORT_READDIR64_COMPAT,
    SUPPORT_READDIR64_R_COMPAT,
  };

/* Returns the last supported function.  May exclude
   SUPPORT_READDIR64_R_COMPAT if not implemented.  */
enum support_readdir_op support_readdir_op_last (void);

/* Returns the name of the function that corresponds to the OP constant.  */
const char *support_readdir_function (enum support_readdir_op op);

/* Returns the d_ino field width for OP, in bits.  */
unsigned int support_readdir_inode_width (enum support_readdir_op op);

/* Returns the d_off field width for OP, in bits.  Zero if not present.  */
unsigned int support_readdir_offset_width (enum support_readdir_op op);

/* Returns true if OP is an _r variant with name length restrictions.  */
bool support_readdir_r_variant (enum support_readdir_op op);

/* First, free E->d_name and set the field to NULL.  Then call the
   readdir variant as specified by OP.  If successfully, copy fields
   to E, make a copy of the entry name using strdup, and write its
   addres sto E->d_name.

   Return true if an entry was read, or false if the end of the
   directory stream was reached.  Terminates the process upon error.
   The caller is expected to free E->d_name if the function is not
   called again for this E.

   Note that this function assumes that E->d_name has been initialized
   to NULL or has been allocated by a previous call to this function.  */
bool support_readdir (DIR *stream, enum support_readdir_op op,
                      struct support_dirent *e) __nonnull ((1, 3));

/* Checks that the readdir operation OP fails with errno value EXPECTED.  */
void support_readdir_expect_error (DIR *stream, enum support_readdir_op op,
                                   int expected) __nonnull ((1));

__END_DECLS

#endif /* SUPPORT_READDIR_H */
