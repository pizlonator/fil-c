/* Definitions of status bits for `wait' et al.
   MIPS version, based on the generic version (bits/waitstatus.h).

   Copyright (C) 1992-2026 Free Software Foundation, Inc.
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

#if !defined _SYS_WAIT_H && !defined _STDLIB_H
# error "Never include <bits/waitstatus.h> directly; use <sys/wait.h> instead."
#endif


/* On MIPS SIGRTMAX is 127, so we need to handle the status code 127
   which is impossible on other ports.  */

/* If WIFEXITED(STATUS), the low-order 8 bits of the status.  */
#define        __WEXITSTATUS(status)   (((status) & 0xff00) >> 8)

/* If WIFSIGNALED(STATUS), the terminating signal.  */
#define        __WTERMSIG(status)      ((status) & 0x7f)

/* If WIFSTOPPED(STATUS), the signal that stopped the child.  */
#define        __WSTOPSIG(status)      __WEXITSTATUS(status)

/* Nonzero if STATUS indicates normal termination.  */
#define        __WIFEXITED(status)     (__WTERMSIG(status) == 0)

/* Nonzero if STATUS indicates termination by a signal.  */
static __inline int
__WIFSIGNALED (int __status)
{
  return ((signed char) ((__status & 0x7f) + 1) >> 1) > 0 || __status == 0x7f;
}

/* Nonzero if STATUS indicates the child is stopped.  */
static __inline int
__WIFSTOPPED (int __status)
{
  return (__status & 0xff) == 0x7f && __status != 0x7f;
}

/* Nonzero if STATUS indicates the child continued after a stop.  We only
   define this if <bits/waitflags.h> provides the WCONTINUED flag bit.  */
#ifdef WCONTINUED
# define __WIFCONTINUED(status)        ((status) == __W_CONTINUED)
#endif

/* Nonzero if STATUS indicates the child dumped core.  */
#define        __WCOREDUMP(status)     ((status) & __WCOREFLAG)

/* Macros for constructing status values.  */
#define        __W_EXITCODE(ret, sig)  ((ret) << 8 | (sig))
#define        __W_STOPCODE(sig)       ((sig) << 8 | 0x7f)
#define __W_CONTINUED          0xffff
#define        __WCOREFLAG             0x80
