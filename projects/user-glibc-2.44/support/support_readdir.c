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

#include <support/readdir.h>

#include <dlfcn.h>
#include <stddef.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xdirent.h>

/* Copied from <olddirent.h>.  */
struct __old_dirent64
  {
    __ino_t d_ino;
    __off64_t d_off;
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];
  };

static struct __old_dirent64 *(*readdir64_compat) (DIR *);
static int (*readdir64_r_compat) (DIR *, struct __old_dirent64 *,
                                  struct __old_dirent64 **);

static void __attribute__ ((constructor))
init (void)
{
  /* These compat symbols exists on alpha, i386, m67k , powerpc, sparc
     at the same GLIBC_2.1 version. */
  readdir64_compat = dlvsym (RTLD_DEFAULT, "readdir64", "GLIBC_2.1");
  readdir64_r_compat = dlvsym (RTLD_DEFAULT, "readdir64_r", "GLIBC_2.1");
}

enum support_readdir_op
support_readdir_op_last (void)
{
  if (readdir64_r_compat != NULL)
    {
      TEST_VERIFY (readdir64_compat != NULL);
      return SUPPORT_READDIR64_R_COMPAT;
    }
  else
    return SUPPORT_READDIR64_R;
}

const char *
support_readdir_function (enum support_readdir_op op)
{
  switch (op)
    {
      case SUPPORT_READDIR:
        return "readdir";
      case SUPPORT_READDIR64:
        return "readdir64";
      case SUPPORT_READDIR_R:
        return "readdir_r";
      case SUPPORT_READDIR64_R:
        return "readdir64_r";
      case SUPPORT_READDIR64_COMPAT:
        return "readdir64@GBLIC_2.1";
      case SUPPORT_READDIR64_R_COMPAT:
        return "readdir64_r@GBLIC_2.1";
    }
  FAIL_EXIT1 ("invalid support_readdir_op constant: %d", op);
}

unsigned int
support_readdir_inode_width (enum support_readdir_op op)
{
  switch (op)
    {
      case SUPPORT_READDIR:
      case SUPPORT_READDIR_R:
        return sizeof ((struct dirent) { 0, }.d_ino) * 8;
      case SUPPORT_READDIR64:
      case SUPPORT_READDIR64_R:
        return sizeof ((struct dirent64) { 0, }.d_ino) * 8;
      case SUPPORT_READDIR64_COMPAT:
      case SUPPORT_READDIR64_R_COMPAT:
        return sizeof ((struct __old_dirent64) { 0, }.d_ino) * 8;
    }
  FAIL_EXIT1 ("invalid support_readdir_op constant: %d", op);
}

unsigned int
support_readdir_offset_width (enum support_readdir_op op)
{
#ifdef _DIRENT_HAVE_D_OFF
  switch (op)
    {
    case SUPPORT_READDIR:
    case SUPPORT_READDIR_R:
      return sizeof ((struct dirent) { 0, }.d_off) * 8;
    case SUPPORT_READDIR64:
    case SUPPORT_READDIR64_R:
      return sizeof ((struct dirent64) { 0, }.d_off) * 8;
    case SUPPORT_READDIR64_COMPAT:
    case SUPPORT_READDIR64_R_COMPAT:
      return sizeof ((struct __old_dirent64) { 0, }.d_off) * 8;
    }
#else
  switch (op)
    {
    case SUPPORT_READDIR:
    case SUPPORT_READDIR_R:
    case SUPPORT_READDIR64:
    case SUPPORT_READDIR64_R:
    case SUPPORT_READDIR64_COMPAT:
    case SUPPORT_READDIR64_R_COMPAT:
      return 0;
    }
#endif
  FAIL_EXIT1 ("invalid support_readdir_op constant: %d", op);
}

bool
support_readdir_r_variant (enum support_readdir_op op)
{
  switch (op)
    {
      case SUPPORT_READDIR:
      case SUPPORT_READDIR64:
      case SUPPORT_READDIR64_COMPAT:
        return false;
      case SUPPORT_READDIR_R:
      case SUPPORT_READDIR64_R:
      case SUPPORT_READDIR64_R_COMPAT:
        return true;
    }
  FAIL_EXIT1 ("invalid support_readdir_op constant: %d", op);
}

static bool
copy_dirent (struct support_dirent *dst, struct dirent *src)
{
  if (src == NULL)
    return false;
  dst->d_ino = src->d_ino;
#ifdef _DIRENT_HAVE_D_OFF
  dst->d_off = src->d_off;
#else
  dst->d_off = 0;
#endif
  dst->d_type = src->d_type;
  dst->d_name = xstrdup (src->d_name);
  return true;
}

static bool
copy_dirent64 (struct support_dirent *dst, struct dirent64 *src)
{
  if (src == NULL)
    return false;
  dst->d_ino = src->d_ino;
#ifdef _DIRENT_HAVE_D_OFF
  dst->d_off = src->d_off;
#else
  dst->d_off = 0;
#endif
  dst->d_type = src->d_type;
  dst->d_name = xstrdup (src->d_name);
  return true;
}

static bool
copy_old_dirent64 (struct support_dirent *dst, struct __old_dirent64 *src)
{
  if (src == NULL)
    return false;
  dst->d_ino = src->d_ino;
#ifdef _DIRENT_HAVE_D_OFF
  dst->d_off = src->d_off;
#else
  dst->d_off = 0;
#endif
  dst->d_type = src->d_type;
  dst->d_name = xstrdup (src->d_name);
  return true;
}

bool
support_readdir (DIR *stream, enum support_readdir_op op,
                 struct support_dirent *e)
{
  free (e->d_name);
  e->d_name = NULL;
  switch (op)
    {
    case SUPPORT_READDIR:
      return copy_dirent (e, xreaddir (stream));
    case SUPPORT_READDIR64:
      return copy_dirent64 (e, xreaddir64 (stream));

      /* The functions readdir_r, readdir64_r were deprecated in glibc 2.24.  */
      DIAG_PUSH_NEEDS_COMMENT;
      DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wdeprecated-declarations");

    case SUPPORT_READDIR_R:
      {
        struct dirent buf;
        if (!xreaddir_r (stream, &buf))
          return false;
        return copy_dirent (e, &buf);
      }
    case SUPPORT_READDIR64_R:
      {
        struct dirent64 buf;
        if (!xreaddir64_r (stream, &buf))
          return false;
        return copy_dirent64 (e, &buf);
      }

      DIAG_POP_NEEDS_COMMENT;

    case SUPPORT_READDIR64_COMPAT:
      if (readdir64_compat == NULL)
        FAIL_EXIT1 ("readdir64 compat function not implemented");
      return copy_old_dirent64 (e, readdir64_compat (stream));

    case SUPPORT_READDIR64_R_COMPAT:
      {
        if (readdir64_r_compat == NULL)
          FAIL_EXIT1 ("readdir64_r compat function not implemented");
        struct __old_dirent64 buf;
        struct __old_dirent64 *e1;
        int ret = readdir64_r_compat (stream, &buf, &e1);
        if (ret != 0)
          {
            errno = ret;
            FAIL ("readdir64_r@GLIBC_2.1: %m");
            return false;
          }
        if (e1 == NULL)
          return false;
        return copy_old_dirent64 (e, e1);
      }
    }
  FAIL_EXIT1 ("support_readdir: invalid op argument %d", (int) op);
}

void
support_readdir_expect_error (DIR *stream, enum support_readdir_op op,
                              int expected)
{
  switch (op)
    {
    case SUPPORT_READDIR:
      errno = 0;
      TEST_VERIFY (readdir (stream) == NULL);
      TEST_COMPARE (errno, expected);
      return;
    case SUPPORT_READDIR64:
      errno = 0;
      TEST_VERIFY (readdir64 (stream) == NULL);
      TEST_COMPARE (errno, expected);
      return;

      /* The functions readdir_r, readdir64_r were deprecated in glibc 2.24.  */
      DIAG_PUSH_NEEDS_COMMENT;
      DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wdeprecated-declarations");

    case SUPPORT_READDIR_R:
      {
        struct dirent buf;
        struct dirent *e;
        errno = readdir_r (stream, &buf, &e);
        TEST_COMPARE (errno, expected);;
      }
      return;
    case SUPPORT_READDIR64_R:
      {
        struct dirent64 buf;
        struct dirent64 *e;
        errno = readdir64_r (stream, &buf, &e);
        TEST_COMPARE (errno, expected);;
      }
      return;

      DIAG_POP_NEEDS_COMMENT;

    case SUPPORT_READDIR64_COMPAT:
      if (readdir64_compat == NULL)
        FAIL_EXIT1 ("readdir64_r compat function not implemented");
      errno = 0;
      TEST_VERIFY (readdir64_compat (stream) == NULL);
      TEST_COMPARE (errno, expected);
      return;
    case SUPPORT_READDIR64_R_COMPAT:
      {
        if (readdir64_r_compat == NULL)
          FAIL_EXIT1 ("readdir64_r compat function not implemented");
        struct __old_dirent64 buf;
        struct __old_dirent64 *e;
        errno = readdir64_r_compat (stream, &buf, &e);
        TEST_COMPARE (errno, expected);
      }
      return;
    }
  FAIL_EXIT1 ("support_readdir_expect_error: invalid op argument %d",
              (int) op);
}
