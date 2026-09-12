/* Error-checking wrappers for <dirent.h>
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

#ifndef SUPPORT_XDIRENT_H
#define SUPPORT_XDIRENT_H

#include <dirent.h>
#include <errno.h>
#include <libc-diag.h>
#include <stdbool.h>
#include <stddef.h>

__BEGIN_DECLS

DIR *xopendir (const char *path);
DIR *xfdopendir (int fd);
void xclosedir (DIR *);

void *support_readdir_check (const char *, void *, int);

static __attribute__ ((unused)) struct dirent *
xreaddir (DIR *stream)
{
  int saved_errno = errno;
  errno = 0;
  struct dirent *result = readdir (stream);
  return support_readdir_check ("readdir", result, saved_errno);
}

static __attribute__ ((unused)) struct dirent64 *
xreaddir64 (DIR *stream)
{
  int saved_errno = errno;
  errno = 0;
  struct dirent64 *result = readdir64 (stream);
  return support_readdir_check ("readdir64", result, saved_errno);
}

/* The functions readdir_r, readdir64_r were deprecated in glibc 2.24.  */
DIAG_PUSH_NEEDS_COMMENT;
DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wdeprecated-declarations");

int support_readdir_r_check (const char *, int, void *, void *);

static __attribute__ ((unused)) bool
xreaddir_r (DIR *stream, struct dirent *buf)
{
  struct dirent *ptr;
  int ret = readdir_r (stream, buf, &ptr);
  if (ret == 0 && ptr == NULL)
    return false;
  support_readdir_r_check ("readdir_r", ret, buf, ptr);
  return true;
}

static __attribute__ ((unused)) bool
xreaddir64_r (DIR *stream, struct dirent64 *buf)
{
  struct dirent64 *ptr;
  int ret = readdir64_r (stream, buf, &ptr);
  if (ret == 0 && ptr == NULL)
    return false;
  support_readdir_r_check ("readdir64_r", ret, buf, ptr);
  return true;
}

DIAG_POP_NEEDS_COMMENT;

__END_DECLS

#endif /* SUPPORT_XDIRENT_H */
