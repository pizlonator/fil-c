/* Linux sigaltstack syscall implementation, routed through Fil-C.
   Copyright (C) 2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <signal.h>
#include <pizlonated_syscalls.h>

/* The raw SYS_sigaltstack stub bypasses Fil-C's syscall dispatch, so route the
   call through zsys_sigaltstack (the same pattern as pause.c / pread64.c). This
   is the glibc twin of the usermusl routing, so sigaltstack works under both
   libcs rather than only musl.  */
int
__sigaltstack (const stack_t *ss, stack_t *old)
{
  return zsys_sigaltstack (ss, old);
}
weak_alias (__sigaltstack, sigaltstack)
