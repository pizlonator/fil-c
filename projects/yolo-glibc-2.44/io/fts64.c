/* File tree traversal functions LFS version.
   Copyright (C) 2015-2026 Free Software Foundation, Inc.
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

#define FTS_OPEN __fts64_open
#define FTS_CLOSE __fts64_close
#define FTS_READ __fts64_read
#define FTS_SET __fts64_set
#define FTS_CHILDREN __fts64_children
#define FTSOBJ FTS64
#define FTSENTRY FTSENT64
#define INO_T ino64_t
#define STRUCT_STAT stat64
#define FSTAT __fstat64
#define FSTATAT __fstatat64
#define STRUCT_STATFS statfs64
#define FSTATFS __fstatfs64

#define fts_open __rename_fts_open
#define fts_close __rename_fts_close
#define fts_read __rename_fts_read
#define fts_set __rename_fts_set
#define fts_children __rename_fts_children

#include "fts-common.c"

#undef fts_open
#undef fts_close
#undef fts_read
#undef fts_set
#undef fts_children

weak_alias (__fts64_open, fts64_open)
weak_alias (__fts64_close, fts64_close)
weak_alias (__fts64_read, fts64_read)
weak_alias (__fts64_set, fts64_set)
weak_alias (__fts64_children, fts64_children)

#ifdef __OFF_T_MATCHES_OFF64_T
weak_alias (__fts64_open, fts_open)
weak_alias (__fts64_close, fts_close)
weak_alias (__fts64_read, fts_read)
weak_alias (__fts64_set, fts_set)
weak_alias (__fts64_children, fts_children)
#endif
