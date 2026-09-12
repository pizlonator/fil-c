/* _dl_thp_madvise.  Linux version.
   Copyright (C) 2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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

#ifndef _DL_THP_MADVISE_H
#define _DL_THP_MADVISE_H

/* Similar to madvise, but disable THP if the madvise syscall returns
   -EINVAL which indicates that THP isn't supported by kernel.  */

static inline int
_dl_thp_madvise (void *addr, size_t size)
{
  int res = INTERNAL_SYSCALL_CALL (madvise, addr, size, MADV_HUGEPAGE);
  if (__glibc_unlikely (INTERNAL_SYSCALL_ERROR_P (res))
      && INTERNAL_SYSCALL_ERRNO (res) == EINVAL)
    {
      /* NB: Disable THP if THP isn't supported by kernel.  */
      GL(dl_thp_mode) = thp_mode_not_supported;
      GL(dl_elf_thp_control) = dl_elf_thp_control_disabled;
      return INTERNAL_SYSCALL_ERRNO (res);
    }
  return res;
}

#endif /* _DL_THP_MADVISE_H  */
