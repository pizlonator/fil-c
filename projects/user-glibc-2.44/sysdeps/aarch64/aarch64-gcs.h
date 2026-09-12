/* AArch64 GCS (Guarded Control Stack) declarations.
   This file is part of the GNU C Library.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.

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

#ifndef _AARCH64_GCS_H
#define _AARCH64_GCS_H

#include <stddef.h>
#include <stdbool.h>

struct gcs_record
{
  void *gcs_base;
  void *gcs_token;
  size_t gcs_size;
};

void *__alloc_gcs (size_t, struct gcs_record *) attribute_hidden;

static inline bool
has_gcs (void)
{
  register unsigned long x16 asm ("x16") = 1;
  asm ("hint	40" /* chkfeat x16 */ : "+r" (x16));
  return x16 == 0;
}

#endif
