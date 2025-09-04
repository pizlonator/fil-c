/* rtld-audit version check.  AArch64 version.
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

static inline bool
_dl_audit_check_version (unsigned int lav)
{
  /* Audit version 1 do not save x8 or NEON registers, which required
     changing La_aarch64_regs and La_aarch64_retval layout (BZ#26643).  The
     missing indirect result save/restore makes _dl_runtime_profile
     potentially trigger undefined behavior if the function returns a large
     struct (even when PLT trace is not requested).  */
  return lav == LAV_CURRENT;
}
